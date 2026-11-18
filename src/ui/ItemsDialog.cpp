#include "ItemsDialog.h"

#include "Format.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
// Sorts the size column by bytes rather than by its text.
class SizeItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem &other) const override
    {
        if (treeWidget() && treeWidget()->sortColumn() == 0)
            return data(0, Qt::UserRole).toLongLong() < other.data(0, Qt::UserRole).toLongLong();
        return QTreeWidgetItem::operator<(other);
    }
};
} // namespace

ItemsView::ItemsView(QWidget *parent)
    : QWidget(parent)
    , m_summary(new QLabel(this))
    , m_tree(new QTreeWidget(this))
{
    m_summary->setWordWrap(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setHeaderLabels({tr("Size"), tr("Path")});
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::DescendingOrder);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_summary);
    layout->addWidget(m_tree, 1);
}

void ItemsView::setResult(const QString &folderPath, const ScanResult &result)
{
    if (!result.valid)
        m_summary->setText(tr("The folder has not been scanned yet."));
    else
        m_summary->setText(tr("%n item(s), %1 in all, last scanned %2.", nullptr,
                              static_cast<int>(result.items.size()))
                               .arg(Format::bytes(result.cleanableBytes),
                                    QLocale().toString(result.finishedAt, QLocale::ShortFormat)));

    const QDir base(folderPath);
    QList<QTreeWidgetItem *> rows;
    for (const ScanItem &item : result.items) {
        auto *row = new SizeItem;
        row->setText(0, Format::bytes(item.bytes));
        row->setData(0, Qt::UserRole, item.bytes);
        row->setTextAlignment(0, Qt::AlignRight | Qt::AlignVCenter);
        row->setText(1, QDir::toNativeSeparators(base.relativeFilePath(item.path)));
        row->setToolTip(1, QDir::toNativeSeparators(item.path));
        rows.append(row);
    }
    m_tree->clear();
    m_tree->addTopLevelItems(rows);
    m_tree->resizeColumnToContents(0);
}

ItemsDialog::ItemsDialog(const QString &folderName, const QString &folderPath,
                         const ScanResult &result, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("What cleaning %1 deletes").arg(folderName));
    resize(720, 480);

    auto *items = new ItemsView(this);
    items->setResult(folderPath, result);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(items, 1);
    layout->addWidget(buttons);
}
