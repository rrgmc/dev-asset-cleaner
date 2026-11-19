#include "CleanDetailsDialog.h"

#include "ItemsDialog.h"
#include "SubfolderPanel.h"
#include "clean/Monitor.h"
#include "model/Store.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

CleanDetailsDialog::CleanDetailsDialog(Store *store, Monitor *monitor, const QUuid &folderId,
                                       Tab tab, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_monitor(monitor)
    , m_folderId(folderId)
    , m_tabs(new QTabWidget(this))
    , m_items(new ItemsView)
    , m_cleanButton(new QPushButton(tr("&Clean…")))
{
    const Folder *f = m_store->folder(folderId);
    setWindowTitle(tr("What cleaning %1 deletes").arg(f ? f->name : QString()));
    resize(720, 520);

    // A Whole folder deletes everything in it, often thousands of items: show only the totals.
    m_items->setSummaryOnly(f && f->kind == Folder::Kind::Whole);
    auto *itemsPage = new QWidget;
    auto *itemsLayout = new QVBoxLayout(itemsPage);
    itemsLayout->addWidget(m_items);
    m_tabs->addTab(itemsPage, tr("&Items"));
    if (f && f->kind == Folder::Kind::Matching) {
        m_subfolderPage = new QWidget;
        // Holds the panel once the tab is first shown.
        new QVBoxLayout(m_subfolderPage);
        m_tabs->addTab(m_subfolderPage, tr("&Subfolders"));
        connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
            if (m_tabs->widget(index) == m_subfolderPage)
                subfolderPanel();
        });
    }

    updateItems();
    updateCleanButton();
    connect(m_monitor, &Monitor::folderChanged, this, [this](const QUuid &id) {
        if (id != m_folderId)
            return;
        updateItems();
        updateCleanButton();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_cleanButton, QDialogButtonBox::ActionRole);
    // Enter must not clean: Close stays the default button.
    m_cleanButton->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Close)->setDefault(true);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_cleanButton, &QPushButton::clicked, this,
            [this] { emit cleanRequested(m_folderId); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs, 1);
    layout->addWidget(buttons);

    showTab(tab);
}

void CleanDetailsDialog::showTab(Tab tab)
{
    if (tab == Tab::Subfolders && m_subfolderPage)
        m_tabs->setCurrentWidget(m_subfolderPage);
    else if (tab == Tab::Items)
        m_tabs->setCurrentIndex(0);
}

SubfolderPanel *CleanDetailsDialog::subfolderPanel()
{
    if (!m_subfolders && m_subfolderPage) {
        m_subfolders = new SubfolderPanel(m_store, m_monitor, m_folderId, m_subfolderPage);
        m_subfolders->setConfirmCleaning(m_confirmCleaning);
        m_subfolderPage->layout()->setContentsMargins(0, 0, 0, 0);
        m_subfolderPage->layout()->addWidget(m_subfolders);
    }
    return m_subfolders;
}

void CleanDetailsDialog::setConfirmCleaning(bool confirm)
{
    m_confirmCleaning = confirm;
    if (m_subfolders)
        m_subfolders->setConfirmCleaning(confirm);
}

void CleanDetailsDialog::updateItems()
{
    const Folder *f = m_store->folder(m_folderId);
    if (f)
        m_items->setResult(f->path, m_monitor->result(m_folderId));
}

void CleanDetailsDialog::updateCleanButton()
{
    // The same rule as the folder list's Clean button (FolderModel::CanCleanRole).
    const ScanResult r = m_monitor->result(m_folderId);
    m_cleanButton->setEnabled(m_store->folder(m_folderId)
                              && m_monitor->state(m_folderId) == Monitor::State::Idle
                              && r.valid && r.cleanableBytes > 0);
}
