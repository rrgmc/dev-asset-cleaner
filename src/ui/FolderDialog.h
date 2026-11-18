#pragma once

#include "model/Folder.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class SizeSpinBox;
class Store;

// Edits a folder: its name, path, kind, cleaner types, excluded subfolders and warning size.
class FolderDialog : public QDialog
{
    Q_OBJECT

public:
    FolderDialog(Store *store, const Folder &folder, QWidget *parent = nullptr);

    Folder folder() const;

private:
    Folder::Kind kind() const;
    void browse();
    void chooseExcludes();
    void updateOk();

    Folder m_folder;
    QLineEdit *m_name;
    QLineEdit *m_path;
    QComboBox *m_kind;
    QLabel *m_kindHint;
    QListWidget *m_types;
    QLabel *m_excludes;
    SizeSpinBox *m_warn;
    QPushButton *m_ok = nullptr;
};
