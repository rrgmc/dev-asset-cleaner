#include "SystemFoldersDialog.h"

#include "ItemCheckStyle.h"
#include "model/Store.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int IndexRole = Qt::UserRole + 1;
} // namespace

SystemFoldersDialog::SystemFoldersDialog(Store *store, const QList<SystemFolder> &folders,
                                         QWidget *parent)
    : QDialog(parent)
    , m_folders(folders)
    , m_list(new QTreeWidget(this))
    , m_note(new QLabel(this))
{
    setWindowTitle(tr("Add system folders"));
    resize(680, 420);

    auto *hint = new QLabel(tr("Trash and cache folders of the operating system that are safe to "
                               "empty. Each one is added as a whole folder, and nothing is "
                               "deleted until you clean it."),
                            this);
    hint->setWordWrap(true);

    m_list->setHeaderLabels({tr("Folder"), tr("Path")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->header()->setStretchLastSection(true);
    ItemCheckStyle::apply(m_list);
    for (int i = 0; i < m_folders.size(); ++i) {
        const SystemFolder &f = m_folders.at(i);
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(1, QDir::toNativeSeparators(f.path));
        item->setToolTip(0, f.note);
        item->setToolTip(1, f.note);
        item->setData(0, IndexRole, i);
        if (store->folderWithPath(f.path)) {
            item->setText(0, tr("%1 (already in the list)").arg(f.name));
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable));
            item->setCheckState(0, Qt::Unchecked);
        } else {
            item->setText(0, f.name);
            item->setCheckState(0, Qt::Checked);
        }
    }
    m_list->resizeColumnToContents(0);
    connect(m_list, &QTreeWidget::itemChanged, this, &SystemFoldersDialog::updateButtons);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &SystemFoldersDialog::updateNote);

    m_note->setWordWrap(true);
    if (m_folders.isEmpty())
        m_note->setText(tr("No system folders were found on this computer."));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("&Add"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_note);
    layout->addWidget(buttons);

    if (m_list->topLevelItemCount() > 0)
        m_list->setCurrentItem(m_list->topLevelItem(0));
    updateButtons();
}

QList<SystemFolder> SystemFoldersDialog::chosen() const
{
    QList<SystemFolder> result;
    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *item = m_list->topLevelItem(i);
        if ((item->flags() & Qt::ItemIsEnabled) && item->checkState(0) == Qt::Checked)
            result.append(m_folders.at(item->data(0, IndexRole).toInt()));
    }
    return result;
}

void SystemFoldersDialog::updateNote()
{
    const QTreeWidgetItem *item = m_list->currentItem();
    if (item)
        m_note->setText(m_folders.at(item->data(0, IndexRole).toInt()).note);
}

void SystemFoldersDialog::updateButtons()
{
    m_ok->setEnabled(!chosen().isEmpty());
}
