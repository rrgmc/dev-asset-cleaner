#include "FreeSpaceDialog.h"

#include "Format.h"
#include "clean/Monitor.h"
#include "model/Store.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
const QColor WarningColor(0xD9, 0x48, 0x0F);

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
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->header()->setStretchLastSection(true);
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
    refresh();
}

void FreeSpaceDialog::refresh()
{
    m_list->clear();
    const QList<Volume> volumes = m_monitor->volumes();
    for (const Volume &v : volumes) {
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(DiskColumn, v.low ? QStringLiteral("⚠ ") + v.name : v.name);
        item->setText(FreeColumn, Format::bytes(v.freeBytes));
        item->setText(SizeColumn, Format::bytes(v.totalBytes));
        if (v.totalBytes > 0) {
            const double used = 100.0 * double(v.totalBytes - v.freeBytes) / double(v.totalBytes);
            item->setText(UsedColumn, tr("%1%").arg(used, 0, 'f', 0));
        }
        item->setText(PathColumn, QDir::toNativeSeparators(v.rootPath));
        for (int c : {FreeColumn, SizeColumn, UsedColumn})
            item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        if (v.low) {
            for (int c = 0; c < m_list->columnCount(); ++c)
                item->setForeground(c, WarningColor);
            item->setToolTip(DiskColumn, tr("Free space is below the warning in the settings."));
        }
    }
    for (int c = 0; c < PathColumn; ++c)
        m_list->resizeColumnToContents(c);

    const qint64 warnBelow = m_store->settings().freeWarnBytes;
    QString note;
    if (volumes.isEmpty())
        note = tr("No monitored folder is on a disk that can be read.");
    else if (warnBelow > 0)
        note = tr("Disks with less than %1 free are marked. The limit is in the settings.")
                   .arg(Format::bytes(warnBelow));
    else
        note = tr("The free space warning is off in the settings.");
    m_note->setText(note);
}
