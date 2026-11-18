#pragma once

#include <QAbstractListModel>
#include <QUuid>

class Monitor;
class Store;

// The folders of the chosen groups, group by group, with their last scan. A filter keeps the
// folders whose name or path contains its text.
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

private:
    void reload();

    Store *m_store;
    Monitor *m_monitor;
    QString m_filter;
    QList<QUuid> m_groupIds;
    QList<QUuid> m_ids;
};
