#pragma once

#include <QAbstractListModel>
#include <QUuid>

class Monitor;
class Store;

// The store's groups, in order, with the totals of their folders' last scans. The name can be
// edited in place.
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

private:
    void reload();

    Store *m_store;
    Monitor *m_monitor;
    QList<QUuid> m_ids;
};
