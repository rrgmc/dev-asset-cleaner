#pragma once

#include "clean/Scanner.h"

#include <QDialog>
#include <QWidget>

class QLabel;
class QTreeWidget;

// Lists what cleaning a folder would delete, from a scan result, largest first. In summary
// mode it shows only the space and the number of items, in large type, without the list: a
// Whole folder such as a temporary folder can hold thousands of items.
class ItemsView : public QWidget
{
    Q_OBJECT

public:
    explicit ItemsView(QWidget *parent = nullptr);

    // Shows `result`, with paths relative to `folderPath`.
    void setResult(const QString &folderPath, const ScanResult &result);
    // Shows the summary instead of the list. Call it before setResult.
    void setSummaryOnly(bool summaryOnly);
    bool isSummaryOnly() const { return m_summaryOnly; }

private:
    QLabel *m_summary;
    QTreeWidget *m_tree;
    QWidget *m_totals;
    QLabel *m_bytes;
    QLabel *m_count;
    bool m_summaryOnly = false;
};

// An ItemsView in a dialog of its own.
class ItemsDialog : public QDialog
{
    Q_OBJECT

public:
    ItemsDialog(const QString &folderName, const QString &folderPath, const ScanResult &result,
                QWidget *parent = nullptr);
};
