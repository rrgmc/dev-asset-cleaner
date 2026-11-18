#include "GroupModel.h"

#include "Format.h"
#include "clean/Monitor.h"
#include "model/Store.h"

GroupModel::GroupModel(Store *store, Monitor *monitor, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_monitor(monitor)
{
    connect(m_store, &Store::changed, this, &GroupModel::reload);
    connect(m_monitor, &Monitor::summaryChanged, this, [this] {
        if (!m_ids.isEmpty())
            emit dataChanged(index(0), index(rowCount() - 1));
    });
    reload();
}

int GroupModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_ids.size());
}

QVariant GroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_ids.size())
        return {};
    const QUuid id = m_ids.at(index.row());
    const Group *g = m_store->group(id);
    if (!g)
        return {};

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return g->name;
    case Qt::ToolTipRole: {
        const QList<Folder> folders = m_store->foldersIn({id});
        return tr("%n folder(s), %1 to clean", nullptr, static_cast<int>(folders.size()))
            .arg(Format::bytes(m_monitor->groupCleanableBytes({id})));
    }
    case IdRole:
        return id;
    case CountRole:
        return static_cast<int>(m_store->foldersIn({id}).size());
    case CleanableRole:
        return m_monitor->groupCleanableBytes({id});
    case WarningRole:
        for (const Folder &f : m_store->foldersIn({id})) {
            const ScanResult r = m_monitor->result(f.id);
            if (!r.error.isEmpty() || (f.warnBytes > 0 && r.cleanableBytes > f.warnBytes))
                return true;
        }
        return false;
    default:
        return {};
    }
}

bool GroupModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_ids.size())
        return false;
    const QString name = value.toString().trimmed();
    if (name.isEmpty())
        return false;
    m_store->renameGroup(m_ids.at(index.row()), name);
    return true;
}

Qt::ItemFlags GroupModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsEditable | Qt::ItemIsDropEnabled;
    return f;
}

QUuid GroupModel::idAt(int row) const
{
    return row >= 0 && row < m_ids.size() ? m_ids.at(row) : QUuid();
}

int GroupModel::rowOf(const QUuid &id) const
{
    return static_cast<int>(m_ids.indexOf(id));
}

void GroupModel::reload()
{
    QList<QUuid> ids;
    for (const Group &g : m_store->groups())
        ids.append(g.id);
    // A rename or a folder change keeps the rows, and with them the selection.
    if (ids == m_ids) {
        if (!m_ids.isEmpty())
            emit dataChanged(index(0), index(rowCount() - 1));
        return;
    }
    beginResetModel();
    m_ids = ids;
    endResetModel();
}
