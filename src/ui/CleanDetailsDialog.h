#pragma once

#include <QDialog>
#include <QUuid>

class ItemsView;
class Monitor;
class QTabWidget;
class Store;
class SubfolderModel;
class SubfolderPanel;

// Shows what cleaning a folder deletes, in two tabs: the items from the folder's last scan,
// kept up to date as Monitor rescans it, and, for a Matching folder only, a SubfolderPanel to
// clean it one subfolder at a time. The Subfolders tab scans the subfolders only once it is
// first shown.
class CleanDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Tab { Items, Subfolders };

    CleanDetailsDialog(Store *store, Monitor *monitor, const QUuid &folderId, Tab tab,
                       QWidget *parent = nullptr);

    // Whether the folder has a Subfolders tab: it is a Matching folder.
    bool hasSubfolders() const { return m_subfolderPage != nullptr; }
    // The Subfolders tab's panel, made now if it was not yet shown; null without the tab.
    SubfolderPanel *subfolderPanel();
    // Tests turn off the question before cleaning a subfolder.
    void setConfirmCleaning(bool confirm);
    QTabWidget *tabs() const { return m_tabs; }

private:
    void updateItems();

    Store *m_store;
    Monitor *m_monitor;
    QUuid m_folderId;
    QTabWidget *m_tabs;
    ItemsView *m_items;
    QWidget *m_subfolderPage = nullptr;
    SubfolderPanel *m_subfolders = nullptr;
    bool m_confirmCleaning = true;
};
