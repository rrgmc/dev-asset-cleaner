#include "FreeSpaceDialog.h"

#include "Format.h"
#include "Icons.h"
#include "ItemCheckStyle.h"
#include "clean/Monitor.h"
#include "model/Store.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {
enum Column { DiskColumn, FreeColumn, SizeColumn, UsedColumn, PathColumn };
} // namespace

FreeSpaceDialog::FreeSpaceDialog(Store *store, Monitor *monitor, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_monitor(monitor)
    , m_list(new QTreeWidget(this))
    , m_note(new QLabel(this))
{
    setWindowTitle(tr("Free space"));
    resize(620, 300);

    m_list->setHeaderLabels({tr("Disk"), tr("Free"), tr("Size"), tr("Used"), tr("Path")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    ItemCheckStyle::apply(m_list);
    m_list->headerItem()->setToolTip(DiskColumn, tr("Checked disks warn when free space is low"));
    // The disk and path columns share the width left over by the numbers.
    QHeaderView *header = m_list->header();
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(header->fontMetrics().horizontalAdvance(QStringLiteral("000,0 GB")) + 16);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(DiskColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(PathColumn, QHeaderView::Stretch);
    for (int c : {FreeColumn, SizeColumn, UsedColumn})
        m_list->headerItem()->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);

    m_note->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_note);
    layout->addWidget(buttons);

    connect(m_monitor, &Monitor::volumesChanged, this, &FreeSpaceDialog::refresh);
    connect(m_store, &Store::changed, this, &FreeSpaceDialog::refresh);
    connect(m_store, &Store::settingsChanged, this, &FreeSpaceDialog::refresh);
    // Queued: changing the settings rebuilds the list, which must not happen inside the item's
    // own signal.
    connect(m_list, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem *item, int column) {
                if (column != DiskColumn)
                    return;
                const QString root = item->data(DiskColumn, Qt::UserRole).toString();
                const bool on = item->checkState(DiskColumn) == Qt::Checked;
                QMetaObject::invokeMethod(this, [this, root, on] { setDiskWarns(root, on); },
                                          Qt::QueuedConnection);
            });
    refresh();
}

void FreeSpaceDialog::setDiskWarns(const QString &rootPath, bool on)
{
    Settings s = m_store->settings();
    s.freeAlertOffDisks.removeAll(rootPath);
    if (!on)
        s.freeAlertOffDisks.append(rootPath);
    m_store->setSettings(s);
}

void FreeSpaceDialog::refresh()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    const QList<Volume> volumes = m_monitor->volumes();
    const Settings &settings = m_store->settings();
    for (const Volume &v : volumes) {
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(DiskColumn, v.low() ? QStringLiteral("⚠ ") + v.name : v.name);
        item->setData(DiskColumn, Qt::UserRole, v.rootPath);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(DiskColumn,
                            settings.freeAlertOn(v.rootPath) ? Qt::Checked : Qt::Unchecked);
        item->setText(FreeColumn, Format::bytes(v.freeBytes));
        item->setText(SizeColumn, Format::bytes(v.totalBytes));
        if (v.totalBytes > 0) {
            const double used = 100.0 * double(v.totalBytes - v.freeBytes) / double(v.totalBytes);
            item->setText(UsedColumn, tr("%1%").arg(used, 0, 'f', 0));
        }
        item->setText(PathColumn, QDir::toNativeSeparators(v.rootPath));
        for (int c : {FreeColumn, SizeColumn, UsedColumn})
            item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        if (v.low()) {
            for (int c = 0; c < m_list->columnCount(); ++c)
                item->setForeground(c, Icons::severityTextColor(v.level));
            item->setToolTip(DiskColumn, tr("Free space is at the %1 level of the warning.")
                                             .arg(severityName(v.level)));
        }
    }

    const bool on = std::any_of(settings.freeAlert.limits.cbegin(),
                                settings.freeAlert.limits.cend(), [](qint64 l) { return l > 0; });
    QString note;
    if (volumes.isEmpty())
        note = tr("No monitored folder is on a disk that can be read.");
    else if (on)
        note = tr("Checked disks warn when free space is low, in the colour of the level. The "
                  "levels are in the settings.");
    else
        note = tr("The free space warning is off in the settings.");
    m_note->setText(note);
}
