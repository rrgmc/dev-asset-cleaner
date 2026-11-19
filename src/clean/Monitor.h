#pragma once

#include "Scanner.h"
#include "model/Alert.h"
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
    // The free space warning's level. None when the disk is left out of the warning.
    Severity level = Severity::None;

    bool low() const { return level != Severity::None; }
};

struct Warnings
{
    QList<Volume> lowVolumes;   // the disks with a free space level
    QList<QUuid> largeFolders;  // in the warnings, with more to clean than their own limit
    // The space to clean in the folders in the warnings (warningCleanableBytes) against the
    // settings' levels. A folder over its own limit makes it at least Low.
    Severity cleanLevel = Severity::None;
    // The most serious level of the disks.
    Severity freeLevel = Severity::None;

    bool any() const { return cleanLevel != Severity::None || freeLevel != Severity::None; }
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
    // Scans the folder again and deletes what it finds, or moves it to the trash. Refused
    // while the folder is busy, and for unsafe Whole folders (see Scanner::unsafeWholeReason).
    void clean(const QUuid &id, CleanMethod method);
    void cleanAll(CleanMethod method);
    // The same for the folders of these groups only.
    void rescanGroups(const QList<QUuid> &groupIds);
    void cleanGroups(const QList<QUuid> &groupIds, CleanMethod method);
    // Asks running work to stop and waits for it.
    void cancelAll();

    // What cleaning would free, over every folder's last scan.
    qint64 cleanableBytes() const;
    // The same over the folders of these groups.
    qint64 groupCleanableBytes(const QList<QUuid> &groupIds) const;
    // The same over the folders that count in the warnings (Folder::inWarnings).
    qint64 warningCleanableBytes() const;

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

    void cleanFolders(const QList<Folder> &folders, CleanMethod method);
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
