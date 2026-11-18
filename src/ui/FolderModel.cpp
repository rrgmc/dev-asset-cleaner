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
        const int row = rowOf(id);
        if (row >= 0)
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
    case Qt::ToolTipRole:
        return QDir::toNativeSeparators(f->path);
    case IdRole:
        return id;
    case PathRole:
        return QDir::toNativeSeparators(f->path);
    case TypesRole: {
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
        return tr("%1 in the folder").arg(Format::bytes(result.totalBytes));
    case CleanableRole:
        return result.cleanableBytes;
    case TotalRole:
        return result.totalBytes;
    case ScannedRole:
        return result.valid;
    case BusyRole:
        return state != Monitor::State::Idle;
    case WarningRole:
        return !result.error.isEmpty() || (f->warnBytes > 0 && result.totalBytes > f->warnBytes);
    case CanCleanRole:
        return state == Monitor::State::Idle && result.valid && result.cleanableBytes > 0;
    default:
        return {};
    }
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

void FolderModel::reload()
{
    beginResetModel();
    m_ids.clear();
    for (const Folder &f : m_store->folders()) {
        if (m_filter.isEmpty() || f.name.contains(m_filter, Qt::CaseInsensitive)
            || QDir::toNativeSeparators(f.path).contains(m_filter, Qt::CaseInsensitive)
            || f.path.contains(m_filter, Qt::CaseInsensitive))
            m_ids.append(f.id);
    }
    endResetModel();
}
