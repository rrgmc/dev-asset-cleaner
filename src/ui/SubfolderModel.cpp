#include "SubfolderModel.h"

#include "FolderModel.h"
#include "Format.h"
#include "model/Store.h"

#include <QDir>

#include <algorithm>

namespace {
constexpr int MaxThreads = 2;  // as Monitor: scans are bound by the disk
}

SubfolderModel::SubfolderModel(Store *store, Monitor *monitor, const QUuid &folderId,
                               QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_monitor(monitor)
    , m_folderId(folderId)
{
    m_pool.setMaxThreadCount(MaxThreads);
    // Cleaning the whole folder disables the rows' Clean buttons.
    connect(m_monitor, &Monitor::folderChanged, this, [this](const QUuid &id) {
        if (id == m_folderId && !m_rows.isEmpty())
            emit dataChanged(index(0), index(rowCount() - 1));
    });
    reload();
}

SubfolderModel::~SubfolderModel()
{
    cancelAll();
}

int SubfolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SubfolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return r.name;
    case Qt::ToolTipRole:
    case FolderModel::PathRole:
        return QDir::toNativeSeparators(r.path);
    case FolderModel::TypesRole:
        if (r.excluded)
            return tr("Excluded");
        if (r.result.valid && !r.result.items.isEmpty()) {
            if (r.result.items.size() == 1 && r.result.items.first().path == r.path)
                return tr("All of it");
            return tr("%n folder(s) to delete", nullptr, static_cast<int>(r.result.items.size()));
        }
        return {};
    case FolderModel::StatusRole:
        if (r.state == Monitor::State::Scanning)
            return tr("Scanning…");
        if (r.state == Monitor::State::Cleaning)
            return tr("Cleaning…");
        if (!r.result.error.isEmpty())
            return r.result.error;
        if (!r.result.valid)
            return tr("Not scanned");
        return tr("%1 in the folder").arg(Format::bytes(r.result.totalBytes));
    case FolderModel::CleanableRole:
        return r.result.cleanableBytes;
    case FolderModel::TotalRole:
        return r.result.totalBytes;
    case FolderModel::ScannedRole:
        return r.result.valid;
    case FolderModel::BusyRole:
        return r.state != Monitor::State::Idle;
    case FolderModel::WarningRole:
        return !r.result.error.isEmpty();
    case FolderModel::CanCleanRole:
        return r.state == Monitor::State::Idle && !r.excluded && r.result.valid
               && r.result.cleanableBytes > 0 && !folderCleaning();
    default:
        return {};
    }
}

QString SubfolderModel::nameAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).name : QString();
}

QString SubfolderModel::pathAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).path : QString();
}

ScanResult SubfolderModel::resultAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).result : ScanResult{};
}

bool SubfolderModel::isExcluded(int row) const
{
    return row >= 0 && row < m_rows.size() && m_rows.at(row).excluded;
}

int SubfolderModel::rowOf(const QString &name) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).name == name)
            return i;
    }
    return -1;
}

bool SubfolderModel::isBusy() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(),
                       [](const Row &r) { return r.state != Monitor::State::Idle; });
}

bool SubfolderModel::folderCleaning() const
{
    return m_monitor->state(m_folderId) == Monitor::State::Cleaning;
}

void SubfolderModel::reload()
{
    cancelAll();
    beginResetModel();
    m_rows.clear();
    if (const Folder *f = m_store->folder(m_folderId); f && f->kind == Folder::Kind::Matching) {
        // Links are left out: scans never follow them, so there is nothing to clean in one.
        const QFileInfoList entries = QDir(f->path).entryInfoList(
            QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot | QDir::NoSymLinks,
            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &e : entries) {
#ifdef Q_OS_WIN
            if (e.isJunction())
                continue;
#endif
            Row r;
            r.name = e.fileName();
            r.path = e.filePath();
            r.excluded = f->excludes.contains(r.name, Qt::CaseInsensitive);
            m_rows.append(r);
        }
    }
    endResetModel();
    rescanAll();
}

void SubfolderModel::rescan(int row)
{
    start(row, false);
}

void SubfolderModel::rescanAll()
{
    for (int row = 0; row < m_rows.size(); ++row)
        start(row, false);
}

void SubfolderModel::clean(int row)
{
    if (row < 0 || row >= m_rows.size() || m_rows.at(row).excluded || folderCleaning())
        return;
    start(row, true);
}

void SubfolderModel::start(int row, bool cleaning)
{
    const Folder *f = m_store->folder(m_folderId);
    if (!f || f->kind != Folder::Kind::Matching || row < 0 || row >= m_rows.size())
        return;
    Row &r = m_rows[row];
    if (r.state != Monitor::State::Idle)
        return;
    r.state = cleaning ? Monitor::State::Cleaning : Monitor::State::Scanning;
    r.cancel = std::make_shared<std::atomic_bool>(false);
    const int generation = ++r.generation;
    // An excluded subfolder is only sized: with no types, its scan finds nothing to delete.
    const QList<CleanerType> types = r.excluded ? QList<CleanerType>{} : m_store->typesFor(*f);
    const auto cancel = r.cancel;
    m_pool.start([this, parent = f->path, name = r.name, types, cancel, generation,
                                cleaning] {
        const CancelCheck canceled = [cancel] { return cancel->load(); };
        ScanResult result = Scanner::scanSubfolder(parent, name, types, canceled);
        CleanResult cleanResult;
        if (cleaning) {
            // The scan just above is the fresh one cleaning needs: delete what it found.
            if (result.valid && result.error.isEmpty())
                cleanResult = Scanner::clean(result.items, canceled);
            result = Scanner::scanSubfolder(parent, name, types, canceled);
        }
        QMetaObject::invokeMethod(this, [this, name, generation, result, cleanResult, cleaning] {
            finish(name, generation, result, cleaning ? &cleanResult : nullptr);
        });
    });
    emit dataChanged(index(row), index(row));
}

void SubfolderModel::finish(const QString &name, int generation, const ScanResult &result,
                            const CleanResult *cleanResult)
{
    const int row = rowOf(name);
    if (row < 0 || m_rows.at(row).generation != generation)
        return;
    Row &r = m_rows[row];
    r.state = Monitor::State::Idle;
    if (result.valid)
        r.result = result;
    emit dataChanged(index(row), index(row));
    if (cleanResult)
        emit cleaned(name, *cleanResult);
}

void SubfolderModel::cancelAll()
{
    for (Row &r : m_rows) {
        if (r.cancel)
            r.cancel->store(true);
    }
    m_pool.waitForDone();
    // The finished jobs queued their results; drop them with the stale generation.
    for (int row = 0; row < m_rows.size(); ++row) {
        Row &r = m_rows[row];
        ++r.generation;
        if (r.state != Monitor::State::Idle) {
            r.state = Monitor::State::Idle;
            emit dataChanged(index(row), index(row));
        }
    }
}
