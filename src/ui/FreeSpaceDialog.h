#pragma once

#include <QDialog>

class Monitor;
class QLabel;
class QTreeWidget;
class Store;

// Shows the free space on each disk that holds a monitored folder, and marks the disks at a
// level of the free space warning in the settings. Each disk's check box says whether it warns
// at all (Settings::freeAlertOffDisks). Follows the monitor's updates while it is open.
class FreeSpaceDialog : public QDialog
{
    Q_OBJECT

public:
    FreeSpaceDialog(Store *store, Monitor *monitor, QWidget *parent = nullptr);

private:
    void refresh();
    void setDiskWarns(const QString &rootPath, bool on);

    Store *m_store;
    Monitor *m_monitor;
    QTreeWidget *m_list;
    QLabel *m_note;
};
