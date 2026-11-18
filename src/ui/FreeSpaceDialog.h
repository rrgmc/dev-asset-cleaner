#pragma once

#include <QDialog>

class Monitor;
class QLabel;
class QTreeWidget;
class Store;

// Shows the free space on each disk that holds a monitored folder, and marks the disks below
// the free space warning in the settings. Follows the monitor's updates while it is open.
class FreeSpaceDialog : public QDialog
{
    Q_OBJECT

public:
    FreeSpaceDialog(Store *store, Monitor *monitor, QWidget *parent = nullptr);

private:
    void refresh();

    Store *m_store;
    Monitor *m_monitor;
    QTreeWidget *m_list;
    QLabel *m_note;
};
