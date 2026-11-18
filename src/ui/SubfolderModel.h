#pragma once

#include "clean/Monitor.h"
#include "clean/Scanner.h"

#include <QAbstractListModel>
#include <QThreadPool>
#include <QUuid>

#include <atomic>
#include <memory>
#include <optional>

class Store;

// The first-level subfolders of one Matching folder, each with its own scan, for cleaning them
// one at a time. Answers FolderModel's roles, so FolderDelegate paints its rows. Scans and
// cleans run in the background on a pool of its own; Monitor keeps the folder's own result.
//
// A subfolder sees what scan() of the whole folder would find in it (Scanner::scanSubfolder).
// The folder's excluded subfolders are listed and sized but never cleaned.
class SubfolderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    SubfolderModel(Store *store, Monitor *monitor, const QUuid &folderId,
                   QObject *parent = nullptr);
    ~SubfolderModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QUuid folderId() const { return m_folderId; }
    QString nameAt(int row) const;
    QString pathAt(int row) const;
    ScanResult resultAt(int row) const;
    bool isExcluded(int row) const;
    int rowOf(const QString &name) const;
    bool isBusy() const;

    // Lists the subfolders again and scans them all.
    void reload();
    void rescan(int row);
    void rescanAll();
    // Scans the subfolder again and deletes what it finds, or moves it to the trash. Refused
    // while it is busy, for an excluded subfolder, and while Monitor cleans the whole folder.
    void clean(int row, CleanMethod method);
    // Asks running work to stop and waits for it.
    void cancelAll();

signals:
    void cleaned(const QString &name, const CleanResult &result);

private:
    struct Row
    {
        QString name;
        QString path;
        bool excluded = false;
        Monitor::State state = Monitor::State::Idle;
        ScanResult result;
        std::shared_ptr<std::atomic_bool> cancel;
        int generation = 0;
    };

    // Scans the row; with a method, cleans it that way first.
    void start(int row, std::optional<CleanMethod> cleaning);
    void finish(const QString &name, int generation, const ScanResult &result,
                const CleanResult *cleanResult);
    bool folderCleaning() const;

    Store *m_store;
    Monitor *m_monitor;
    QUuid m_folderId;
    QList<Row> m_rows;
    QThreadPool m_pool;
};
