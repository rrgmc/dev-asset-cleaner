#pragma once

#include "model/Folder.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class SizeSpinBox;
class Store;

// Edits a folder: its name, path, cleaner types and warning size.
class FolderDialog : public QDialog
{
    Q_OBJECT

public:
    FolderDialog(Store *store, const Folder &folder, QWidget *parent = nullptr);

    Folder folder() const;

private:
    void browse();
    void chooseExcludes();
    void typeChanged(int row);
    void updateOk();

    Folder m_folder;
    QLineEdit *m_name;
    QLineEdit *m_path;
    QListWidget *m_types;
    QLabel *m_excludes;
    SizeSpinBox *m_warn;
    QPushButton *m_ok = nullptr;
};
