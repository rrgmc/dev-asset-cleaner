#pragma once

#include "SortOrder.h"

#include <QAbstractListModel>
#include <QUuid>

class Monitor;
class Store;

// The folders of the chosen groups, with their last scan. In manual order they come group by
// group; the other orders mix the groups. A filter keeps the folders whose name or path
// contains its text.
class FolderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,         // native separators
        TypesRole,        // type names, joined
        StatusRole,       // what the right side of the row shows under the space to clean
        CleanableRole,    // qint64
        ScannedRole,      // bool: a scan has finished
        BusyRole,         // bool: scanning or cleaning
        WarningRole,      // bool: more to clean than its warning size, or the scan failed
        CanCleanRole,     // bool
        GroupRole,        // the group's name
    };

    FolderModel(Store *store, Monitor *monitor, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    const QList<QUuid> &groups() const { return m_groupIds; }
    void setGroups(const QList<QUuid> &groupIds);

    QString filter() const { return m_filter; }
    void setFilter(const QString &text);

    QUuid idAt(int row) const;
    int rowOf(const QUuid &id) const;
    QUuid groupIdAt(int row) const;

    SortOrder sortOrder() const { return m_sortOrder; }
    void setSortOrder(SortOrder order);

private:
    void reload();
    // Puts the rows in the sort order, keeping the selection and the current row.
    void resort();
    QList<QUuid> sortedIds() const;

    Store *m_store;
    Monitor *m_monitor;
    QString m_filter;
    QList<QUuid> m_groupIds;
    SortOrder m_sortOrder = SortOrder::Manual;
    QList<QUuid> m_manualIds;  // the store's order
    QList<QUuid> m_ids;        // the rows
};
