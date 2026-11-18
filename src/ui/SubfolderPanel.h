#pragma once

#include <QWidget>
#include <QUuid>

class Monitor;
class QComboBox;
class QLabel;
class QSortFilterProxyModel;
class Store;
class SubfolderListView;
class SubfolderModel;
struct CleanResult;

// Lists the first-level subfolders of a Matching folder as the main window lists folders, each
// with what cleaning would free in it and a Clean button, so the user can clean the folder one
// subfolder at a time. Asks Monitor to rescan the folder after each clean. A tab of
// CleanDetailsDialog.
class SubfolderPanel : public QWidget
{
    Q_OBJECT

public:
    SubfolderPanel(Store *store, Monitor *monitor, const QUuid &folderId,
                   QWidget *parent = nullptr);

    SubfolderModel *model() const { return m_model; }
    // Tests turn off the question before cleaning.
    void setConfirmCleaning(bool confirm) { m_confirmCleaning = confirm; }
    // Cleans the subfolder at `row` of the model, after asking.
    void cleanRow(int row);

private:
    void showItems(int row);
    void showContextMenu(int row, const QPoint &globalPos);
    void onCleaned(const QString &name, const CleanResult &result);
    void updateStatus();

    Store *m_store;
    Monitor *m_monitor;
    QUuid m_folderId;
    SubfolderModel *m_model;
    QSortFilterProxyModel *m_proxy;
    SubfolderListView *m_list;
    QComboBox *m_sort;
    QLabel *m_status;
    QString m_message;  // the last clean's outcome
    bool m_confirmCleaning = true;
};
