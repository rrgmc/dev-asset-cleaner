#pragma once

#include <QPoint>

class QAbstractItemView;
class QPainter;

// Where a row dragged within its own list lands: just before or just after another row. The
// lists draw a line there while the drag is over them.
namespace ReorderDrop {

struct Target {
    int row = -1;  // the row the dragged one lands next to; -1 for none
    bool after = false;

    bool isValid() const { return row >= 0; }
    bool operator==(const Target &other) const = default;
};

// The row under pos and the half of it pos is in. Below the last row is after the last row.
Target targetAt(const QAbstractItemView *view, const QPoint &pos, int rowCount);
// Draws the insertion line in the view's viewport.
void paint(QPainter &painter, const QAbstractItemView *view, const Target &target);

} // namespace ReorderDrop
