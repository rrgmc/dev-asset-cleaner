#pragma once

#include "clean/Scanner.h"

#include <QDialog>
#include <QWidget>

class QLabel;
class QTreeWidget;

// Lists what cleaning a folder would delete, from a scan result, largest first.
class ItemsView : public QWidget
{
    Q_OBJECT

public:
    explicit ItemsView(QWidget *parent = nullptr);

    // Shows `result`, with paths relative to `folderPath`.
    void setResult(const QString &folderPath, const ScanResult &result);

private:
    QLabel *m_summary;
    QTreeWidget *m_tree;
};

// An ItemsView in a dialog of its own.
class ItemsDialog : public QDialog
{
    Q_OBJECT

public:
    ItemsDialog(const QString &folderName, const QString &folderPath, const ScanResult &result,
                QWidget *parent = nullptr);
};
