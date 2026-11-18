#include "FolderModel.h"

#include "Format.h"
#include "clean/Monitor.h"
#include "model/Store.h"

#include <QDir>

FolderModel::FolderModel(Store *store, Monitor *monitor, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_monitor(monitor)
{
    connect(m_store, &Store::changed, this, &FolderModel::reload);
    connect(m_monitor, &Monitor::folderChanged, this, [this](const QUuid &id) {
        if (rowOf(id) < 0)
            return;
        if (m_sortOrder == SortOrder::Cleanable)
            resort();
        const int row = rowOf(id);
        emit dataChanged(index(row), index(row));
    });
    reload();
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_ids.size());
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_ids.size())
        return {};
    const QUuid id = m_ids.at(index.row());
    const Folder *f = m_store->folder(id);
    if (!f)
        return {};
    const ScanResult result = m_monitor->result(id);
    const Monitor::State state = m_monitor->state(id);

    switch (role) {
    case Qt::DisplayRole:
        return f->name;
    case Qt::ToolTipRole: {
        const Group *g = m_store->group(f->groupId);
        return QStringLiteral("%1\n%2").arg(QDir::toNativeSeparators(f->path),
                                             tr("Group: %1").arg(g ? g->name : QString()));
    }
    case GroupRole: {
        const Group *g = m_store->group(f->groupId);
        return g ? g->name : QString();
    }
    case IdRole:
        return id;
    case PathRole:
        return QDir::toNativeSeparators(f->path);
    case TypesRole: {
        if (f->kind != Folder::Kind::Matching)
            return Folder::kindName(f->kind);
        QStringList names;
        for (const CleanerType &t : m_store->typesFor(*f))
            names.append(t.name);
        return names.join(QStringLiteral(", "));
    }
    case StatusRole:
        if (state == Monitor::State::Scanning)
            return tr("Scanning…");
        if (state == Monitor::State::Cleaning)
            return tr("Cleaning…");
        if (!result.error.isEmpty())
            return result.error;
        if (!result.valid)
            return tr("Not scanned");
        return QString();
    case CleanableRole:
        return result.cleanableBytes;
    case ScannedRole:
        return result.valid;
    case BusyRole:
        return state != Monitor::State::Idle;
    case WarningRole:
        return !result.error.isEmpty() || (f->warnBytes > 0 && result.cleanableBytes > f->warnBytes);
    case CanCleanRole:
        return state == Monitor::State::Idle && result.valid && result.cleanableBytes > 0;
    default:
        return {};
    }
}

Qt::ItemFlags FolderModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled;
    return f;
}

void FolderModel::setGroups(const QList<QUuid> &groupIds)
{
    if (groupIds == m_groupIds)
        return;
    m_groupIds = groupIds;
    reload();
}

void FolderModel::setFilter(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed == m_filter)
        return;
    m_filter = trimmed;
    reload();
}

QUuid FolderModel::idAt(int row) const
{
    return row >= 0 && row < m_ids.size() ? m_ids.at(row) : QUuid();
}

int FolderModel::rowOf(const QUuid &id) const
{
    return static_cast<int>(m_ids.indexOf(id));
}

QUuid FolderModel::groupIdAt(int row) const
{
    const Folder *f = m_store->folder(idAt(row));
    return f ? f->groupId : QUuid();
}

void FolderModel::setSortOrder(SortOrder order)
{
    if (order == m_sortOrder)
        return;
    m_sortOrder = order;
    resort();
}

void FolderModel::reload()
{
    beginResetModel();
    m_manualIds.clear();
    for (const Folder &f : m_store->foldersIn(m_groupIds)) {
        if (m_filter.isEmpty() || f.name.contains(m_filter, Qt::CaseInsensitive)
            || QDir::toNativeSeparators(f.path).contains(m_filter, Qt::CaseInsensitive)
            || f.path.contains(m_filter, Qt::CaseInsensitive))
            m_manualIds.append(f.id);
    }
    m_ids = sortedIds();
    endResetModel();
}

QList<QUuid> FolderModel::sortedIds() const
{
    return Sorting::sorted(
        m_manualIds, m_sortOrder,
        [this](const QUuid &id) {
            const Folder *f = m_store->folder(id);
            return f ? f->name : QString();
        },
        [this](const QUuid &id) { return m_monitor->result(id).cleanableBytes; });
}

void FolderModel::resort()
{
    const QList<QUuid> ids = sortedIds();
    if (ids == m_ids)
        return;
    emit layoutAboutToBeChanged();
    const QModelIndexList before = persistentIndexList();
    QModelIndexList after;
    after.reserve(before.size());
    for (const QModelIndex &i : before)
        after.append(index(static_cast<int>(ids.indexOf(m_ids.value(i.row())))));
    m_ids = ids;
    changePersistentIndexList(before, after);
    emit layoutChanged();
}
