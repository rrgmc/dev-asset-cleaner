#include "ReorderDrop.h"

#include <QAbstractItemView>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

namespace ReorderDrop {

Target targetAt(const QAbstractItemView *view, const QPoint &pos, int rowCount)
{
    if (rowCount <= 0)
        return {};
    const QModelIndex index = view->indexAt(pos);
    if (!index.isValid()) {
        // Below the last row.
        const QModelIndex last = view->model()->index(rowCount - 1, 0);
        if (pos.y() >= view->visualRect(last).bottom())
            return {rowCount - 1, true};
        return {};
    }
    const QRect rect = view->visualRect(index);
    return {index.row(), pos.y() >= rect.center().y()};
}

void paint(QPainter &painter, const QAbstractItemView *view, const Target &target)
{
    if (!target.isValid())
        return;
    // The style's drop indicator, as QAbstractItemView draws it (and favdir shows): a rect of no
    // height is a line between rows.
    const QRect rect = view->visualRect(view->model()->index(target.row, 0));
    const int y = target.after ? rect.bottom() + 1 : rect.top();
    QStyleOption option;
    option.initFrom(view);
    option.rect = QRect(rect.left(), y, rect.width(), 0);
    view->style()->drawPrimitive(QStyle::PE_IndicatorItemViewItemDrop, &option, &painter, view);
}

} // namespace ReorderDrop
