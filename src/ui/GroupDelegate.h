#pragma once

#include <QStyledItemDelegate>

// Paints a group row: its name on the left and what cleaning its folders would free on the
// right. Editing renames the group.
class GroupDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
