#pragma once

#include "clean/Scanner.h"

#include <QMainWindow>
#include <QUuid>

class FolderListView;
class FolderModel;
class Monitor;
class QLabel;
class QLineEdit;
class QMenu;
class Store;
class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Store *store, Monitor *monitor, QWidget *parent = nullptr);
    ~MainWindow() override;

    void showAndFocus();
    void toggleVisible();
    void showMessage(const QString &text, bool error = false);

    // Adds a folder with the given types, or selects it when it is in the list already.
    void addFolderPath(const QString &path, const QStringList &typeIds = {},
                       const QString &name = {});

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
    void showAbout();
    void hideWindow();
    void saveGeometryState();

    void addFolder();
    // Appends the tool caches found on this computer.
    void fillAddMenu(QMenu *menu);
    void editFolder(const QUuid &id);
    void removeFolder();
    void rescanFolder();
    void cleanFolder(const QUuid &id);
    void cleanAll();
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
    QLineEdit *m_filter;
    QLabel *m_summary;
    TrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    int m_trayAttempts = 0;
    bool m_trayUpdatePending = false;
    bool m_confirmCleaning = true;
    QString m_lastWarning;  // the warning the tray last notified about
    QList<QAction *> m_folderActions;  // need a selected folder
    QAction *m_cleanAction = nullptr;
    QAction *m_cleanAllAction = nullptr;
};
