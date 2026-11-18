#pragma once

#include "Alert.h"
#include "CleanerType.h"
#include "Folder.h"
#include "Group.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

struct Settings
{
    static Alert defaultCleanAlert();
    static Alert defaultFreeAlert();

    bool closeToTray = true;       // closing the window hides it instead of quitting
    int rescanMinutes = 30;        // rescan every folder this often; 0 turns it off
    // The space to clean in all folders.
    Alert cleanAlert = defaultCleanAlert();
    // The free space on each disk that holds a monitored folder.
    Alert freeAlert = defaultFreeAlert();
    // Root paths of the disks that freeAlert leaves out.
    QStringList freeAlertOffDisks;
    bool deleteDirectly = false;   // cleaning deletes for good instead of moving to the trash

    bool freeAlertOn(const QString &rootPath) const { return !freeAlertOffDisks.contains(rootPath); }

    friend bool operator==(const Settings &, const Settings &) = default;
};

// Owns the groups and their folders, the custom cleaner types and the settings, and persists
// them as JSON. Every folder belongs to one of the groups.
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

    // Loads the file. A missing file is not an error: the store starts with one empty group,
    // "Default", and isNew() returns true. A file from before groups puts its folders in a
    // "Default" group. On a parse error the store is left empty, without groups.
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr);
    bool isNew() const { return m_isNew; }

    void setAutoSave(bool on) { m_autoSave = on; }
    void flush();  // saves now if a save is pending

    const QList<Group> &groups() const { return m_groups; }
    const Group *group(const QUuid &id) const;
    QUuid addGroup(const QString &name);  // appended to the end; returns its id
    void renameGroup(const QUuid &id, const QString &name);
    // Removes the group and stops monitoring its folders. Nothing on disk is deleted.
    void removeGroup(const QUuid &id);
    void moveGroupBy(const QUuid &id, int delta);
    // Moves the group to just before or just after another group.
    void moveGroupNextTo(const QUuid &id, const QUuid &targetId, bool after);
    static QString defaultGroupName();

    // Every folder. A group's folders keep their relative order.
    const QList<Folder> &folders() const { return m_folders; }
    // The folders of these groups, group by group in the groups' order.
    QList<Folder> foldersIn(const QList<QUuid> &groupIds) const;
    const Folder *folder(const QUuid &id) const;
    // The folder with this path (compared cleaned and ignoring case), or nullptr.
    const Folder *folderWithPath(const QString &path) const;

    // Appended to the end of its group; returns its id. Refused (a null id) when the folder's
    // group does not exist.
    QUuid addFolder(Folder folder);
    void updateFolder(const Folder &folder);  // keeps the folder's group
    void removeFolder(const QUuid &id);
    // Moves the folder up or down among its group's folders.
    void moveBy(const QUuid &id, int delta);
    // Moves the folder to just before or just after another folder of the same group. Refused
    // when the target is in another group.
    void moveNextTo(const QUuid &id, const QUuid &targetId, bool after);
    // Moves the folder to the end of another group.
    void moveToGroup(const QUuid &id, const QUuid &groupId);

    // Built-in types followed by the custom ones.
    QList<CleanerType> types() const;
    const QList<CleanerType> &customTypes() const { return m_customTypes; }
    void setCustomTypes(const QList<CleanerType> &types);
    // The type with this id, or nullptr when it no longer exists.
    const CleanerType *type(const QString &id) const;
    // The types of a Matching folder that still exist; none for a Whole folder.
    QList<CleanerType> typesFor(const Folder &folder) const;

    const Settings &settings() const { return m_settings; }
    void setSettings(const Settings &settings);

signals:
    void changed();
    void settingsChanged();
    void saveFailed(const QString &error);

private:
    int rowOf(const QUuid &id) const;
    int groupRowOf(const QUuid &id) const;
    void touch();
    void scheduleSave();

    QString m_filePath;
    QList<Group> m_groups;
    QList<Folder> m_folders;
    QList<CleanerType> m_customTypes;
    Settings m_settings;
    QTimer m_saveTimer;
    bool m_autoSave = true;
    bool m_isNew = false;
};
