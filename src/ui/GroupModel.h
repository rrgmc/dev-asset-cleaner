#pragma once

#include "SortOrder.h"

#include <QAbstractListModel>
#include <QUuid>

class Monitor;
class Store;

// The store's groups, in the chosen sort order, with the totals of their folders' last scans.
// The name can be edited in place.
class GroupModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        CountRole,      // int: folders in the group
        CleanableRole,  // qint64
        WarningRole,    // bool: a folder has more to clean than its warning size, or its scan failed
    };

    GroupModel(Store *store, Monitor *monitor, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QUuid idAt(int row) const;
    int rowOf(const QUuid &id) const;

    SortOrder sortOrder() const { return m_sortOrder; }
    void setSortOrder(SortOrder order);

private:
    void reload();
    // Puts the rows in the sort order, keeping the selection and the current row.
    void resort();
    QList<QUuid> sortedIds() const;

    Store *m_store;
    Monitor *m_monitor;
    SortOrder m_sortOrder = SortOrder::Manual;
    QList<QUuid> m_manualIds;  // the store's order
    QList<QUuid> m_ids;        // the rows
};
