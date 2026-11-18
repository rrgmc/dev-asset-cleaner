#include "CleanableListView.h"

#include "FolderDelegate.h"
#include "FolderModel.h"
#include "Spinner.h"

#include <QMouseEvent>

CleanableListView::CleanableListView(QAbstractItemModel *model, QWidget *parent)
    : QListView(parent)
{
    setModel(model);
    setItemDelegate(new FolderDelegate(this));
    setMouseTracking(true);
    setUniformItemSizes(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_spinnerTimer.setInterval(Spinner::FrameMs);
    connect(&m_spinnerTimer, &QTimer::timeout, this, [this] {
        for (int row = 0; row < this->model()->rowCount(); ++row) {
            const QModelIndex index = this->model()->index(row, 0);
            if (index.data(FolderModel::BusyRole).toBool())
                viewport()->update(visualRect(index));
        }
    });
    connect(model, &QAbstractItemModel::dataChanged, this, &CleanableListView::updateSpinner);
    connect(model, &QAbstractItemModel::modelReset, this, &CleanableListView::updateSpinner);
    connect(model, &QAbstractItemModel::rowsInserted, this, &CleanableListView::updateSpinner);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &CleanableListView::updateSpinner);
    updateSpinner();
}

void CleanableListView::updateSpinner()
{
    bool busy = false;
    for (int row = 0; row < model()->rowCount() && !busy; ++row)
        busy = model()->index(row, 0).data(FolderModel::BusyRole).toBool();
    if (busy && !m_spinnerTimer.isActive())
        m_spinnerTimer.start();
    else if (!busy)
        m_spinnerTimer.stop();
}

QModelIndex CleanableListView::buttonAt(const QPoint &pos) const
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid() || !index.data(FolderModel::CanCleanRole).toBool())
        return {};
    return FolderDelegate::buttonRect(visualRect(index), fontMetrics()).contains(pos)
               ? index
               : QModelIndex();
}

void CleanableListView::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index != m_hoverIndex) {
        if (m_hoverIndex.isValid())
            viewport()->update(visualRect(m_hoverIndex));
        m_hoverIndex = index;
    }
    if (index.isValid())
        viewport()->update(visualRect(index));
    viewport()->setCursor(buttonAt(event->position().toPoint()).isValid() ? Qt::PointingHandCursor
                                                                           : Qt::ArrowCursor);
    QListView::mouseMoveEvent(event);
}

void CleanableListView::mouseReleaseEvent(QMouseEvent *event)
{
    const QPersistentModelIndex button = event->button() == Qt::LeftButton
                                             ? buttonAt(event->position().toPoint())
                                             : QModelIndex();
    QListView::mouseReleaseEvent(event);
    if (button.isValid())
        buttonClicked(button);
}

void CleanableListView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);
    // A double click on the button is two clicks on it, not an activation.
    if (index.isValid() && event->button() == Qt::LeftButton
        && !FolderDelegate::buttonRect(visualRect(index), fontMetrics()).contains(pos)) {
        rowDoubleClicked(index);
        return;
    }
    QListView::mouseDoubleClickEvent(event);
}
