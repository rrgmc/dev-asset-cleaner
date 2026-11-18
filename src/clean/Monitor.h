#pragma once

#include "Scanner.h"
#include "model/Folder.h"

#include <QFuture>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <QUuid>

#include <atomic>
#include <memory>
#include <optional>

class Store;

// The disk a monitored folder is on.
struct Volume
{
    QString rootPath;
    QString name;
    qint64 freeBytes = 0;
    qint64 totalBytes = 0;
    bool low = false;  // below the free space warning
};

struct Warnings
{
    QList<Volume> lowVolumes;
    QList<QUuid> largeFolders;  // over their own warning size
    bool totalTooLarge = false;

    bool any() const { return !lowVolumes.isEmpty() || !largeFolders.isEmpty() || totalTooLarge; }
};

// Scans and cleans the store's folders in the background and keeps the results. Rescans every
// folder on the interval in the settings, and the free space of their disks every minute.
class Monitor : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Scanning, Cleaning };

    explicit Monitor(Store *store, QObject *parent = nullptr);
    ~Monitor() override;

    State state(const QUuid &id) const;
    ScanResult result(const QUuid &id) const;
    bool isBusy() const;

    void rescan(const QUuid &id);
    void rescanAll();
    // Scans the folder again and deletes what its types match. Refused while the folder is
    // busy, and for unsafe "Folder contents" folders (see Scanner::unsafeReason).
    void clean(const QUuid &id);
    void cleanAll();
    // The same for the folders of these groups only.
    void rescanGroups(const QList<QUuid> &groupIds);
    void cleanGroups(const QList<QUuid> &groupIds);
    // Asks running work to stop and waits for it.
    void cancelAll();

    // Totals over every folder's last scan.
    qint64 totalBytes() const;
    qint64 cleanableBytes() const;
    // Totals over the folders of these groups.
    qint64 groupTotalBytes(const QList<QUuid> &groupIds) const;
    qint64 groupCleanableBytes(const QList<QUuid> &groupIds) const;

    QList<Volume> volumes() const;
    Warnings warnings() const;

    // For tools/make_screenshot: made-up results and disks instead of real ones.
    void setResultForDisplay(const QUuid &id, const ScanResult &result);
    void setVolumesForDisplay(const QList<Volume> &volumes);

signals:
    void folderChanged(const QUuid &id);
    void volumesChanged();
    // Emitted when any state, result or volume changes; for summaries such as the tray icon.
    void summaryChanged();
    void cleaned(const QUuid &id, const CleanResult &result);
    void cleanRefused(const QUuid &id, const QString &reason);

private:
    struct Entry
    {
        State state = State::Idle;
        ScanResult result;
        QFuture<void> future;
        std::shared_ptr<std::atomic_bool> cancel;
        int generation = 0;
    };

    void cleanFolders(const QList<Folder> &folders);
    void syncFolders();
    void updateVolumes();
    void applyInterval();
    void finish(const QUuid &id, int generation, const ScanResult &result,
                const CleanResult *cleanResult);

    Store *m_store;
    QHash<QUuid, Entry> m_entries;
    QList<Volume> m_volumes;
    std::optional<QList<Volume>> m_fixedVolumes;
    QTimer m_rescanTimer;
    QTimer m_volumeTimer;
    QThreadPool m_pool;
};
