#pragma once

#include "clean/SystemFolders.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTreeWidget;
class Store;

// Lists the operating system's trash and cache folders found on this computer (see
// SystemFolders) and lets the user pick which ones to add. Folders already in the list are
// shown but cannot be picked.
class SystemFoldersDialog : public QDialog
{
    Q_OBJECT

public:
    SystemFoldersDialog(Store *store, const QList<SystemFolder> &folders,
                        QWidget *parent = nullptr);

    // The folders left checked, in the order given.
    QList<SystemFolder> chosen() const;

private:
    void updateNote();
    void updateButtons();

    QList<SystemFolder> m_folders;
    QTreeWidget *m_list;
    QLabel *m_note;
    QPushButton *m_ok;
};
