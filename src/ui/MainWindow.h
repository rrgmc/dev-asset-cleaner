#pragma once

#include "CleanDetailsDialog.h"
#include "SortOrder.h"
#include "clean/Scanner.h"
#include "model/Alert.h"
#include "model/Folder.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QUuid>

#include <functional>

class FolderListView;
class FolderModel;
class GroupListView;
class GroupModel;
class Monitor;
class QLabel;
class QLineEdit;
class SpinnerWidget;
class QMenu;
class QActionGroup;
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

    // Orders the group list or the folder list, and remembers the choice.
    void setGroupSort(SortOrder order);
    void setFolderSort(SortOrder order);

    // Why the tray icon shows a warning, one line per reason; empty without warnings. The tray
    // menu lists them at its top and the tooltip repeats them.
    QStringList warningReasons() const;

    // Asks before deleting; tests turn the question off.
    void setConfirmCleaning(bool confirm) { m_confirmCleaning = confirm; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
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
    // Asks, then cleans the folder. `from` is the CleanDetailsDialog that asked, if any: the
    // question opens over it, and its Clean subfolders button switches its tab.
    void cleanFolder(const QUuid &id, CleanDetailsDialog *from = nullptr);
    // Opens CleanDetailsDialog on `tab`; the Subfolders tab only for a Matching folder.
    void showCleanDetails(const QUuid &id, CleanDetailsDialog::Tab tab);
    // Cleans the selected groups' folders, or every folder.
    void cleanAll(bool everything = false);
    void addGroup();
    void removeGroups();
    void moveGroup(int delta);
    void moveFoldersToGroup(const QList<QUuid> &folderIds, const QUuid &groupId);
    void fillMoveMenu(QMenu *menu, const QUuid &folderId);
    void updateSortButtons();
    // Adds a submenu with a choice for each sort order. The group's parent is the submenu.
    QActionGroup *addSortMenu(QMenu *parent, const QString &title,
                              const std::function<void(SortOrder)> &apply);
    void showGroupContextMenu(const QUuid &id, const QPoint &globalPos);
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
    // When and to what the window last lost activation. Clicking the notification area icon
    // activates the taskbar first, so toggleVisible needs to know the window was in front.
    QElapsedTimer m_deactivated;
    bool m_lostToTaskbar = false;
    bool m_confirmCleaning = true;
    // The levels of the two warnings at the last tray update, so that only a rise notifies.
    Severity m_lastCleanLevel = Severity::None;
    Severity m_lastFreeLevel = Severity::None;
    QList<QAction *> m_folderActions;  // need a selected folder
    QList<QAction *> m_groupActions;   // need a selected group
    QList<QAction *> m_addActions;     // need a group to add to
    QAction *m_cleanAction = nullptr;
    QAction *m_subfoldersAction = nullptr;  // Matching folders only
    QAction *m_cleanAllAction = nullptr;       // the selected groups
    QAction *m_cleanEverythingAction = nullptr;
    QMenu *m_moveMenu = nullptr;
    QList<QAction *> m_folderMoveActions;  // Move up and down: manual order only
    QList<QAction *> m_groupMoveActions;
    QActionGroup *m_groupSortActions = nullptr;
    QActionGroup *m_folderSortActions = nullptr;
    QAction *m_groupSortButton = nullptr;  // the toolbar's sort buttons
    QAction *m_folderSortButton = nullptr;
};
