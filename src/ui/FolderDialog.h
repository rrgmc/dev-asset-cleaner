#pragma once

#include "model/Folder.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class SizeSpinBox;
class Store;

// Edits a folder: its name, path, cleaner types (Matching folders only), excluded subfolders
// and warning size. The kind is shown but cannot change: it is chosen when the folder is added.
class FolderDialog : public QDialog
{
    Q_OBJECT

public:
    FolderDialog(Store *store, const Folder &folder, QWidget *parent = nullptr);

    Folder folder() const;

private:
    void browse();
    void chooseExcludes();
    void updateOk();

    Folder m_folder;
    QLineEdit *m_name;
    QLineEdit *m_path;
    QListWidget *m_types = nullptr;  // only for Matching folders
    QLabel *m_excludes;
    SizeSpinBox *m_warn;
    QPushButton *m_ok = nullptr;
};
