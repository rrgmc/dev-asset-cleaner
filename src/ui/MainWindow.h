#pragma once

#include "clean/Scanner.h"
#include "model/Folder.h"

#include <QMainWindow>
#include <QUuid>

class FolderListView;
class FolderModel;
class GroupListView;
class GroupModel;
class Monitor;
class QLabel;
class QLineEdit;
class SpinnerWidget;
class QMenu;
class QSplitter;
class Store;
class TrayIcon;
class UsagePanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Store *store, Monitor *monitor, QWidget *parent = nullptr);
    ~MainWindow() override;

    void showAndFocus();
    void toggleVisible();
    void showMessage(const QString &text, bool error = false);

    // Adds a folder of this kind, with the given types and excluded first-level subfolders, to
    // a group (by default the current one), or selects it when it is in a group already.
    void addFolderPath(const QString &path, Folder::Kind kind, const QStringList &typeIds = {},
                       const QString &name = {}, const QUuid &groupId = {},
                       const QStringList &excludes = {});
    // Shows the folder's group, if it is not shown, and selects the folder.
    void selectFolder(const QUuid &id);

    // Asks before deleting; tests turn the question off.
    void setConfirmCleaning(bool confirm) { m_confirmCleaning = confirm; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createActions();
    void createTray();
    void updateTray();
    void updateSummary();
    void updateFolderActions();
    QUuid currentId() const;
    // The groups whose folders the list shows, and the one new folders go to.
    QList<QUuid> selectedGroups() const;
    QUuid currentGroup() const;
    void onGroupsSelected();
    void updateGroupActions();
    void showAbout();
    void hideWindow();
    void saveGeometryState();

    void addFolder();
    // Searches a folder for the types that apply to it; `id` picks a monitored folder.
    void detectTypes(const QUuid &id = {});
    // Offers the operating system's trash and cache folders found on this computer.
    void addSystemFolders();
    // Appends the tool caches found on this computer.
    void fillAddMenu(QMenu *menu);
    void editFolder(const QUuid &id);
    void removeFolder();
    void rescanFolder();
    void cleanFolder(const QUuid &id);
    // Opens SubfolderDialog for a Matching folder.
    void cleanSubfolders(const QUuid &id);
    // Cleans the selected groups' folders, or every folder.
    void cleanAll(bool everything = false);
    void addGroup();
    void removeGroups();
    void moveGroup(int delta);
    void moveFoldersToGroup(const QList<QUuid> &folderIds, const QUuid &groupId);
    void fillMoveMenu(QMenu *menu, const QUuid &folderId);
    void showGroupContextMenu(const QUuid &id, const QPoint &globalPos);
    void showItems();
    void openFolder(const QUuid &id);
    void copyPath(const QUuid &id);
    void editTypes();
    void editSettings();
    void showContextMenu(const QUuid &id, const QPoint &globalPos);
    void onCleaned(const QUuid &id, const CleanResult &result);

    Store *m_store;
    Monitor *m_monitor;
    FolderModel *m_model;
    FolderListView *m_list;
    GroupModel *m_groupModel;
    GroupListView *m_groups;
    QSplitter *m_splitter = nullptr;
    QLineEdit *m_filter;
    QLabel *m_summary;
    QLabel *m_lowSpace;  // disks below the free space warning
    SpinnerWidget *m_busy;
    UsagePanel *m_usage = nullptr;
    TrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    int m_trayAttempts = 0;
    bool m_trayUpdatePending = false;
    bool m_confirmCleaning = true;
    QString m_lastWarning;  // the warning the tray last notified about
    QList<QAction *> m_folderActions;  // need a selected folder
    QList<QAction *> m_groupActions;   // need a selected group
    QList<QAction *> m_addActions;     // need a group to add to
    QAction *m_cleanAction = nullptr;
    QAction *m_subfoldersAction = nullptr;  // Matching folders only
    QAction *m_cleanAllAction = nullptr;       // the selected groups
    QAction *m_cleanEverythingAction = nullptr;
    QMenu *m_moveMenu = nullptr;
};
