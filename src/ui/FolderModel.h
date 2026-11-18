#pragma once

#include <QAbstractListModel>
#include <QUuid>

class Monitor;
class Store;

// The store's folders, in order, with their last scan. A filter keeps the folders whose name or
// path contains its text.
class FolderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,         // native separators
        TypesRole,        // type names, joined
        StatusRole,       // what the right side of the row shows under the size
        CleanableRole,    // qint64
        TotalRole,        // qint64
        ScannedRole,      // bool: a scan has finished
        BusyRole,         // bool: scanning or cleaning
        WarningRole,      // bool: over its warning size, or the scan failed
        CanCleanRole,     // bool
    };

    FolderModel(Store *store, Monitor *monitor, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &text);

    QUuid idAt(int row) const;
    int rowOf(const QUuid &id) const;

private:
    void reload();

    Store *m_store;
    Monitor *m_monitor;
    QString m_filter;
    QList<QUuid> m_ids;
};
