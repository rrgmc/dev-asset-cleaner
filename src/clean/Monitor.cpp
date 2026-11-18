#include "Monitor.h"

#include "model/Store.h"

#include <QDir>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int VolumeIntervalMs = 60 * 1000;
constexpr int MaxThreads = 2;  // scans are bound by the disk, not the processor
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
    const QString path = f->path;
    const QList<CleanerType> types = m_store->typesFor(*f);
    const QStringList excludes = f->excludes;
    const auto cancel = entry.cancel;
    entry.future = QtConcurrent::run(&m_pool, [this, id, generation, path, types, excludes, cancel] {
        const ScanResult result =
            Scanner::scan(path, types, [cancel] { return cancel->load(); }, excludes);
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

void Monitor::clean(const QUuid &id)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    Entry &entry = m_entries[id];
    if (entry.state != State::Idle)
        return;
    const QString path = f->path;
    const QList<CleanerType> types = m_store->typesFor(*f);
    const QStringList excludes = f->excludes;
    const QString reason = Scanner::unsafeReason(path, types);
    if (!reason.isEmpty()) {
        emit cleanRefused(id, reason);
        return;
    }
    entry.state = State::Cleaning;
    entry.cancel = std::make_shared<std::atomic_bool>(false);
    const int generation = ++entry.generation;
    const auto cancel = entry.cancel;
    entry.future = QtConcurrent::run(&m_pool, [this, id, generation, path, types, excludes, cancel] {
        const CancelCheck canceled = [cancel] { return cancel->load(); };
        // Scan again rather than trust an old result: the folder may have changed since.
        const ScanResult before = Scanner::scan(path, types, canceled, excludes);
        CleanResult cleanResult;
        if (before.valid && before.error.isEmpty())
            cleanResult = Scanner::clean(before.items, canceled);
        const ScanResult after = Scanner::scan(path, types, canceled, excludes);
        QMetaObject::invokeMethod(this, [this, id, generation, after, cleanResult] {
            finish(id, generation, after, &cleanResult);
        });
    });
    emit folderChanged(id);
    emit summaryChanged();
}

void Monitor::cleanAll()
{
    cleanFolders(m_store->folders());
}

void Monitor::rescanGroups(const QList<QUuid> &groupIds)
{
    for (const Folder &f : m_store->foldersIn(groupIds))
        rescan(f.id);
    updateVolumes();
}

void Monitor::cleanGroups(const QList<QUuid> &groupIds)
{
    cleanFolders(m_store->foldersIn(groupIds));
}

void Monitor::cleanFolders(const QList<Folder> &folders)
{
    for (const Folder &f : folders) {
        if (result(f.id).cleanableBytes > 0 || !result(f.id).valid)
            clean(f.id);
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

qint64 Monitor::totalBytes() const
{
    qint64 total = 0;
    for (const Folder &f : m_store->folders())
        total += result(f.id).totalBytes;
    return total;
}

qint64 Monitor::cleanableBytes() const
{
    qint64 total = 0;
    for (const Folder &f : m_store->folders())
        total += result(f.id).cleanableBytes;
    return total;
}

qint64 Monitor::groupTotalBytes(const QList<QUuid> &groupIds) const
{
    qint64 total = 0;
    for (const Folder &f : m_store->foldersIn(groupIds))
        total += result(f.id).totalBytes;
    return total;
}

qint64 Monitor::groupCleanableBytes(const QList<QUuid> &groupIds) const
{
    qint64 total = 0;
    for (const Folder &f : m_store->foldersIn(groupIds))
        total += result(f.id).cleanableBytes;
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
        if (v.low)
            w.lowVolumes.append(v);
    }
    for (const Folder &f : m_store->folders()) {
        if (f.warnBytes > 0 && result(f.id).totalBytes > f.warnBytes)
            w.largeFolders.append(f.id);
    }
    const qint64 limit = m_store->settings().totalWarnBytes;
    w.totalTooLarge = limit > 0 && totalBytes() > limit;
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
    const qint64 warnBelow = m_store->settings().freeWarnBytes;
    QList<Volume> volumes;
    if (m_fixedVolumes) {
        volumes = *m_fixedVolumes;
        for (Volume &v : volumes)
            v.low = warnBelow > 0 && v.freeBytes < warnBelow;
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
        v.low = warnBelow > 0 && v.freeBytes < warnBelow;
        volumes.append(v);
    }
    const bool changed =
        volumes.size() != m_volumes.size()
        || !std::equal(volumes.cbegin(), volumes.cend(), m_volumes.cbegin(),
                       [](const Volume &a, const Volume &b) {
                           return a.rootPath == b.rootPath && a.freeBytes == b.freeBytes
                                  && a.low == b.low;
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
