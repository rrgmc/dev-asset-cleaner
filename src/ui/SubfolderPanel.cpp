#include "SubfolderPanel.h"

#include "CleanChoice.h"
#include "CleanableListView.h"
#include "FolderModel.h"
#include "Format.h"
#include "ItemsDialog.h"
#include "SubfolderModel.h"
#include "clean/Monitor.h"
#include "model/Store.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace {
constexpr int MaxFailuresShown = 20;
}

// The dialog's list: rows of a proxy over SubfolderModel.
class SubfolderListView : public CleanableListView
{
public:
    SubfolderListView(QSortFilterProxyModel *proxy, SubfolderPanel *dialog)
        : CleanableListView(proxy, dialog)
        , m_proxy(proxy)
        , m_dialog(dialog)
    {
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

    std::function<void(int row)> onActivated;
    std::function<void(int row, const QPoint &globalPos)> onContextMenu;

protected:
    void buttonClicked(const QModelIndex &index) override
    {
        m_dialog->cleanRow(m_proxy->mapToSource(index).row());
    }

    void rowDoubleClicked(const QModelIndex &index) override
    {
        onActivated(m_proxy->mapToSource(index).row());
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
        if (mods == Qt::NoModifier && currentIndex().isValid()
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
            onActivated(m_proxy->mapToSource(currentIndex()).row());
            return;
        }
        CleanableListView::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        const QModelIndex index =
            event->reason() == QContextMenuEvent::Mouse ? indexAt(event->pos()) : currentIndex();
        if (!index.isValid())
            return;
        setCurrentIndex(index);
        const QPoint global = event->reason() == QContextMenuEvent::Mouse
                                  ? event->globalPos()
                                  : viewport()->mapToGlobal(visualRect(index).bottomLeft());
        onContextMenu(m_proxy->mapToSource(index).row(), global);
    }

private:
    QSortFilterProxyModel *m_proxy;
    SubfolderPanel *m_dialog;
};

SubfolderPanel::SubfolderPanel(Store *store, Monitor *monitor, const QUuid &folderId,
                                 QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_monitor(monitor)
    , m_folderId(folderId)
    , m_model(new SubfolderModel(store, monitor, folderId, this))
    , m_proxy(new QSortFilterProxyModel(this))
    , m_sort(new QComboBox(this))
    , m_status(new QLabel(this))
{
    const Folder *f = m_store->folder(folderId);
    auto *hint = new QLabel(
        tr("The folders inside %1. Each Clean button deletes what the folder's types match in "
           "that subfolder only.")
            .arg(f ? QDir::toNativeSeparators(f->path) : QString()),
        this);
    hint->setWordWrap(true);

    m_proxy->setSourceModel(m_model);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setDynamicSortFilter(true);
    m_list = new SubfolderListView(m_proxy, this);
    m_list->onActivated = [this](int row) { showItems(row); };
    m_list->onContextMenu = [this](int row, const QPoint &pos) { showContextMenu(row, pos); };

    // Each entry's data is the role to sort by; the space to clean sorts largest first.
    m_sort->addItem(tr("Name"), int(Qt::DisplayRole));
    m_sort->addItem(tr("Space to clean"), int(FolderModel::CleanableRole));
    auto applySort = [this] {
        const int role = m_sort->currentData().toInt();
        m_proxy->setSortRole(role);
        m_proxy->sort(0, role == Qt::DisplayRole ? Qt::AscendingOrder : Qt::DescendingOrder);
    };
    // Largest first by default, as the user usually wants the subfolders worth cleaning.
    m_sort->setCurrentIndex(1);
    connect(m_sort, &QComboBox::currentIndexChanged, this, applySort);
    applySort();
    auto *sortLabel = new QLabel(tr("&Sort by:"), this);
    sortLabel->setBuddy(m_sort);

    auto *top = new QHBoxLayout;
    top->addWidget(hint, 1);
    top->addWidget(sortLabel);
    top->addWidget(m_sort);

    auto *rescan = new QPushButton(tr("&Rescan subfolders"), this);
    rescan->setAutoDefault(false);
    connect(rescan, &QPushButton::clicked, this, [this] {
        m_message.clear();
        m_model->reload();
    });

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(m_status, 1);
    bottom->addWidget(rescan);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_list, 1);
    layout->addLayout(bottom);

    connect(m_model, &SubfolderModel::cleaned, this, &SubfolderPanel::onCleaned);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &SubfolderPanel::updateStatus);
    connect(m_model, &QAbstractItemModel::modelReset, this, &SubfolderPanel::updateStatus);
    updateStatus();
}

void SubfolderPanel::updateStatus()
{
    if (m_model->rowCount() == 0) {
        m_status->setText(tr("The folder has no subfolders."));
        return;
    }
    if (m_model->isBusy()) {
        m_status->setText(m_message.isEmpty() ? tr("Scanning…") : m_message);
        return;
    }
    qint64 cleanable = 0;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (!m_model->isExcluded(row))
            cleanable += m_model->resultAt(row).cleanableBytes;
    }
    const QString total = tr("%1 to clean in %n subfolder(s).", nullptr, m_model->rowCount())
                              .arg(Format::bytes(cleanable));
    m_status->setText(m_message.isEmpty() ? total : m_message + QLatin1Char(' ') + total);
}

void SubfolderPanel::cleanRow(int row)
{
    if (!m_model->index(row).data(FolderModel::CanCleanRole).toBool())
        return;
    const QString name = m_model->nameAt(row);
    const ScanResult r = m_model->resultAt(row);
    const QString path = m_model->pathAt(row);
    const CleanMethod method = CleanChoice::method(m_store->settings());
    if (m_confirmCleaning) {
        const bool deletes = CleanChoice::forFolder(method, path) == CleanMethod::Delete;
        const int n = static_cast<int>(r.items.size());
        const QString question =
            deletes ? tr("Delete %n folder(s) that the types match in %1, about %2? This cannot "
                         "be undone.",
                         nullptr, n)
                          .arg(QDir::toNativeSeparators(path), Format::bytes(r.cleanableBytes))
                    : tr("Move %n folder(s) that the types match in %1, about %2, to the %3?",
                         nullptr, n)
                          .arg(QDir::toNativeSeparators(path), Format::bytes(r.cleanableBytes),
                               CleanChoice::trashName());
        QMessageBox box(QMessageBox::Warning, tr("Clean %1").arg(name), question,
                        QMessageBox::Cancel, this);
        QPushButton *details = box.addButton(tr("Show items…"), QMessageBox::HelpRole);
        QPushButton *ok = CleanChoice::addConfirmButton(box, deletes);
        box.setDefaultButton(QMessageBox::Cancel);
        // The Show items button would close the box; reopen it afterwards.
        for (;;) {
            box.exec();
            if (box.clickedButton() != details)
                break;
            ItemsDialog(name, m_model->pathAt(row), r, this).exec();
        }
        if (box.clickedButton() != ok)
            return;
    }
    // The row may have moved while the question was open; the name still finds it.
    m_model->clean(m_model->rowOf(name), method);
}

void SubfolderPanel::showItems(int row)
{
    if (row >= 0 && row < m_model->rowCount())
        ItemsDialog(m_model->nameAt(row), m_model->pathAt(row), m_model->resultAt(row), this).exec();
}

void SubfolderPanel::showContextMenu(int row, const QPoint &globalPos)
{
    const QModelIndex index = m_model->index(row);
    QMenu menu(this);
    menu.addAction(tr("&Clean…"), this, [this, row] { cleanRow(row); })
        ->setEnabled(index.data(FolderModel::CanCleanRole).toBool());
    menu.addAction(tr("&Rescan"), this, [this, row] { m_model->rescan(row); })
        ->setEnabled(!index.data(FolderModel::BusyRole).toBool());
    menu.addAction(tr("&Show what cleaning deletes…"), this, [this, row] { showItems(row); });
    menu.addSeparator();
    menu.addAction(tr("&Open folder"), this, [this, row] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_model->pathAt(row)));
    });
    menu.exec(globalPos);
}

void SubfolderPanel::onCleaned(const QString &name, const CleanResult &result)
{
    m_message = CleanChoice::doneMessage(result, name);
    updateStatus();
    // The main window's numbers come from the folder's own scan.
    m_monitor->rescan(m_folderId);
    if (result.failures.isEmpty())
        return;
    QStringList shown = result.failures.mid(0, MaxFailuresShown);
    if (result.failures.size() > MaxFailuresShown)
        shown.append(tr("…and %n more.", nullptr,
                        static_cast<int>(result.failures.size() - MaxFailuresShown)));
    QMessageBox box(QMessageBox::Warning, tr("Clean %1").arg(name),
                    CleanChoice::failureText(result), QMessageBox::Ok, this);
    box.setInformativeText(CleanChoice::failureHint(result));
    box.setDetailedText(shown.join(QLatin1Char('\n')));
    box.exec();
}
