#include "Monitor.h"

#include "model/Store.h"

#include <QDir>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int VolumeIntervalMs = 60 * 1000;
constexpr int MaxThreads = 2;  // scans are bound by the disk, not the processor

using FolderScan = std::function<ScanResult(const CancelCheck &)>;

// Scans the folder as its kind says, with copies of what that needs, for another thread.
FolderScan folderScan(const Store *store, const Folder &f)
{
    if (f.kind == Folder::Kind::Whole) {
        return [path = f.path, excludes = f.excludes](const CancelCheck &canceled) {
            return Scanner::scanWhole(path, canceled, excludes);
        };
    }
    return [path = f.path, types = store->typesFor(f),
            excludes = f.excludes](const CancelCheck &canceled) {
        return Scanner::scan(path, types, canceled, excludes);
    };
}
} // namespace

Monitor::Monitor(Store *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    m_pool.setMaxThreadCount(MaxThreads);

    connect(&m_rescanTimer, &QTimer::timeout, this, &Monitor::rescanAll);
    m_volumeTimer.setInterval(VolumeIntervalMs);
    connect(&m_volumeTimer, &QTimer::timeout, this, &Monitor::updateVolumes);
    m_volumeTimer.start();

    connect(m_store, &Store::changed, this, &Monitor::syncFolders);
    connect(m_store, &Store::settingsChanged, this, [this] {
        applyInterval();
        updateVolumes();
    });
    syncFolders();
    applyInterval();
}

Monitor::~Monitor()
{
    cancelAll();
}

Monitor::State Monitor::state(const QUuid &id) const
{
    const auto it = m_entries.constFind(id);
    return it == m_entries.cend() ? State::Idle : it->state;
}

ScanResult Monitor::result(const QUuid &id) const
{
    const auto it = m_entries.constFind(id);
    return it == m_entries.cend() ? ScanResult{} : it->result;
}

bool Monitor::isBusy() const
{
    for (const Entry &e : m_entries) {
        if (e.state != State::Idle)
            return true;
    }
    return false;
}

void Monitor::rescan(const QUuid &id)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    Entry &entry = m_entries[id];
    if (entry.state != State::Idle)
        return;
    entry.state = State::Scanning;
    entry.cancel = std::make_shared<std::atomic_bool>(false);
    const int generation = ++entry.generation;
    const FolderScan scan = folderScan(m_store, *f);
    const auto cancel = entry.cancel;
    entry.future = QtConcurrent::run(&m_pool, [this, id, generation, scan, cancel] {
        const ScanResult result = scan([cancel] { return cancel->load(); });
        QMetaObject::invokeMethod(this, [this, id, generation, result] {
            finish(id, generation, result, nullptr);
        });
    });
    emit folderChanged(id);
    emit summaryChanged();
}

void Monitor::rescanAll()
{
    for (const Folder &f : m_store->folders())
        rescan(f.id);
    updateVolumes();
}

void Monitor::clean(const QUuid &id, CleanMethod method)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    Entry &entry = m_entries[id];
    if (entry.state != State::Idle)
        return;
    if (f->kind == Folder::Kind::Whole) {
        const QString reason = Scanner::unsafeWholeReason(f->path);
        if (!reason.isEmpty()) {
            emit cleanRefused(id, reason);
            return;
        }
    }
    entry.state = State::Cleaning;
    entry.cancel = std::make_shared<std::atomic_bool>(false);
    const int generation = ++entry.generation;
    const FolderScan scan = folderScan(m_store, *f);
    const auto cancel = entry.cancel;
    entry.future = QtConcurrent::run(&m_pool, [this, id, generation, scan, cancel, method] {
        const CancelCheck canceled = [cancel] { return cancel->load(); };
        // Scan again rather than trust an old result: the folder may have changed since.
        const ScanResult before = scan(canceled);
        CleanResult cleanResult;
        if (before.valid && before.error.isEmpty())
            cleanResult = Scanner::clean(before.items, method, canceled);
        const ScanResult after = scan(canceled);
        QMetaObject::invokeMethod(this, [this, id, generation, after, cleanResult] {
            finish(id, generation, after, &cleanResult);
        });
    });
    emit folderChanged(id);
    emit summaryChanged();
}

void Monitor::cleanAll(CleanMethod method)
{
    cleanFolders(m_store->folders(), method);
}

void Monitor::rescanGroups(const QList<QUuid> &groupIds)
{
    for (const Folder &f : m_store->foldersIn(groupIds))
        rescan(f.id);
    updateVolumes();
}

void Monitor::cleanGroups(const QList<QUuid> &groupIds, CleanMethod method)
{
    cleanFolders(m_store->foldersIn(groupIds), method);
}

void Monitor::cleanFolders(const QList<Folder> &folders, CleanMethod method)
{
    for (const Folder &f : folders) {
        if (result(f.id).cleanableBytes > 0 || !result(f.id).valid)
            clean(f.id, method);
    }
}

void Monitor::cancelAll()
{
    for (Entry &e : m_entries) {
        if (e.cancel)
            e.cancel->store(true);
    }
    m_pool.waitForDone();
    // The finished jobs queued their results; drop them with the stale generation.
    for (Entry &e : m_entries) {
        ++e.generation;
        e.state = State::Idle;
    }
}

void Monitor::finish(const QUuid &id, int generation, const ScanResult &result,
                     const CleanResult *cleanResult)
{
    const auto it = m_entries.find(id);
    if (it == m_entries.end() || it->generation != generation)
        return;
    it->state = State::Idle;
    if (result.valid)
        it->result = result;
    emit folderChanged(id);
    if (cleanResult)
        emit cleaned(id, *cleanResult);
    updateVolumes();
}

qint64 Monitor::cleanableBytes() const
{
    qint64 total = 0;
    for (const Folder &f : m_store->folders())
        total += result(f.id).cleanableBytes;
    return total;
}

qint64 Monitor::groupCleanableBytes(const QList<QUuid> &groupIds) const
{
    qint64 total = 0;
    for (const Folder &f : m_store->foldersIn(groupIds))
        total += result(f.id).cleanableBytes;
    return total;
}

qint64 Monitor::warningCleanableBytes() const
{
    qint64 total = 0;
    for (const Folder &f : m_store->folders()) {
        if (f.inWarnings)
            total += result(f.id).cleanableBytes;
    }
    return total;
}

QList<Volume> Monitor::volumes() const
{
    return m_volumes;
}

Warnings Monitor::warnings() const
{
    Warnings w;
    for (const Volume &v : m_volumes) {
        if (v.low())
            w.lowVolumes.append(v);
        w.freeLevel = std::max(w.freeLevel, v.level);
    }
    for (const Folder &f : m_store->folders()) {
        if (f.inWarnings && f.warnBytes > 0 && result(f.id).cleanableBytes > f.warnBytes)
            w.largeFolders.append(f.id);
    }
    w.cleanLevel =
        m_store->settings().cleanAlert.levelFor(warningCleanableBytes(), Alert::Direction::Above);
    if (!w.largeFolders.isEmpty())
        w.cleanLevel = std::max(w.cleanLevel, Severity::Low);
    return w;
}

void Monitor::setResultForDisplay(const QUuid &id, const ScanResult &result)
{
    Entry &entry = m_entries[id];
    entry.result = result;
    emit folderChanged(id);
    emit summaryChanged();
}

void Monitor::setVolumesForDisplay(const QList<Volume> &volumes)
{
    m_fixedVolumes = volumes;
    updateVolumes();
}

void Monitor::syncFolders()
{
    QList<QUuid> added;
    QSet<QUuid> current;
    for (const Folder &f : m_store->folders()) {
        current.insert(f.id);
        if (!m_entries.contains(f.id))
            added.append(f.id);
    }
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (!current.contains(it.key())) {
            if (it->cancel)
                it->cancel->store(true);
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
    for (const QUuid &id : added)
        rescan(id);
    updateVolumes();
}

void Monitor::updateVolumes()
{
    const Settings &settings = m_store->settings();
    const auto levelOf = [&settings](const Volume &v) {
        return settings.freeAlertOn(v.rootPath)
                   ? settings.freeAlert.levelFor(v.freeBytes, Alert::Direction::Below)
                   : Severity::None;
    };
    QList<Volume> volumes;
    if (m_fixedVolumes) {
        volumes = *m_fixedVolumes;
        for (Volume &v : volumes)
            v.level = levelOf(v);
    }
    for (const Folder &f : m_fixedVolumes ? QList<Folder>{} : m_store->folders()) {
        QStorageInfo info(f.path);
        if (!info.isValid() || !info.isReady())
            continue;
        const QString root = info.rootPath();
        if (std::any_of(volumes.cbegin(), volumes.cend(),
                        [&root](const Volume &v) { return v.rootPath == root; }))
            continue;
        Volume v;
        v.rootPath = root;
        v.name = info.displayName();
        if (v.name.isEmpty() || v.name == root)
            v.name = QDir::toNativeSeparators(root);
        v.freeBytes = info.bytesAvailable();
        v.totalBytes = info.bytesTotal();
        v.level = levelOf(v);
        volumes.append(v);
    }
    const bool changed =
        volumes.size() != m_volumes.size()
        || !std::equal(volumes.cbegin(), volumes.cend(), m_volumes.cbegin(),
                       [](const Volume &a, const Volume &b) {
                           return a.rootPath == b.rootPath && a.freeBytes == b.freeBytes
                                  && a.level == b.level;
                       });
    m_volumes = volumes;
    if (changed)
        emit volumesChanged();
    emit summaryChanged();
}

void Monitor::applyInterval()
{
    const int minutes = m_store->settings().rescanMinutes;
    if (minutes <= 0) {
        m_rescanTimer.stop();
        return;
    }
    const int ms = minutes * 60 * 1000;
    if (m_rescanTimer.interval() != ms || !m_rescanTimer.isActive())
        m_rescanTimer.start(ms);
}
