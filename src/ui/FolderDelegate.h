#pragma once

#include <QRect>
#include <QStyledItemDelegate>

// Paints a folder row: the name, its types and its path on the left; what cleaning would free
// and the folder's size on the right; then a Clean button.
class FolderDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // The Clean button of a row occupying `rect`, in the same coordinates.
    static QRect buttonRect(const QRect &rect, const QFontMetrics &fm);
    static QString buttonLabel();
};
