#pragma once

#include "CleanerType.h"
#include "Folder.h"

#include <QObject>
#include <QString>
#include <QTimer>

struct Settings
{
    bool closeToTray = true;       // closing the window hides it instead of quitting
    int rescanMinutes = 30;        // rescan every folder this often; 0 turns it off
    qint64 freeWarnBytes = 10LL * 1024 * 1024 * 1024;  // warn below this free space; 0 = off
    qint64 totalWarnBytes = 0;     // warn when all folders together hold more; 0 = off

    friend bool operator==(const Settings &, const Settings &) = default;
};

// Owns the folders, the custom cleaner types and the settings, and persists them as JSON.
//
// Every mutation emits changed() and, when auto-save is on, schedules a debounced save.
class Store : public QObject
{
    Q_OBJECT

public:
    explicit Store(QString filePath, QObject *parent = nullptr);
    ~Store() override;

    static QString defaultFilePath();

    QString filePath() const { return m_filePath; }

    // Loads the file. A missing file is not an error: the store starts empty and isNew()
    // returns true. On a parse error the store is left empty.
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr);
    bool isNew() const { return m_isNew; }

    void setAutoSave(bool on) { m_autoSave = on; }
    void flush();  // saves now if a save is pending

    const QList<Folder> &folders() const { return m_folders; }
    const Folder *folder(const QUuid &id) const;
    // The folder with this path (compared cleaned and ignoring case), or nullptr.
    const Folder *folderWithPath(const QString &path) const;

    QUuid addFolder(Folder folder);  // appended to the end; returns its id
    void updateFolder(const Folder &folder);
    void removeFolder(const QUuid &id);
    void moveBy(const QUuid &id, int delta);

    // Built-in types followed by the custom ones.
    QList<CleanerType> types() const;
    const QList<CleanerType> &customTypes() const { return m_customTypes; }
    void setCustomTypes(const QList<CleanerType> &types);
    // The type with this id, or nullptr when it no longer exists.
    const CleanerType *type(const QString &id) const;
    // The folder's types that still exist. "Folder contents" when it has none.
    QList<CleanerType> typesFor(const Folder &folder) const;

    const Settings &settings() const { return m_settings; }
    void setSettings(const Settings &settings);

signals:
    void changed();
    void settingsChanged();
    void saveFailed(const QString &error);

private:
    int rowOf(const QUuid &id) const;
    void touch();
    void scheduleSave();

    QString m_filePath;
    QList<Folder> m_folders;
    QList<CleanerType> m_customTypes;
    Settings m_settings;
    QTimer m_saveTimer;
    bool m_autoSave = true;
    bool m_isNew = false;
};
