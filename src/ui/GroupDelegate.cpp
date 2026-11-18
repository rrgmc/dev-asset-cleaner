#include "GroupDelegate.h"

#include "Format.h"
#include "GroupModel.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int Margin = 8;
constexpr int VPadding = 5;

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}

const QColor WarningColor(0xD9, 0x48, 0x0F);
} // namespace

QSize GroupDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    const QFontMetrics fm(option.font);
    return QSize(160, fm.height() + 2 * VPadding);
}

void GroupDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // The same selection look as the folder rows.
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const QPalette::ColorGroup group = opt.state & QStyle::State_Active ? QPalette::Active
                                                                          : QPalette::Inactive;
    const QColor base = opt.palette.color(group, QPalette::Base);
    const QColor accent = opt.palette.color(QPalette::Active, QPalette::Highlight);
    const QColor textColor = opt.palette.color(group, QPalette::Text);
    QColor background = base;
    if (selected)
        background = blend(base, accent, group == QPalette::Active ? 0.22 : 0.12);
    else if (hovered)
        background = blend(base, accent, 0.07);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (selected || hovered) {
        QPainterPath path;
        path.addRoundedRect(QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5), 4, 4);
        painter->fillPath(path, background);
        if (selected) {
            painter->setPen(QPen(accent, 1.5));
            painter->drawPath(path);
        }
    }

    const QFontMetrics fm(opt.font);
    const QRect inner = opt.rect.adjusted(Margin, 0, -Margin, 0);
    const qint64 cleanable = index.data(GroupModel::CleanableRole).toLongLong();
    const bool warning = index.data(GroupModel::WarningRole).toBool();
    const QString size = cleanable > 0 ? Format::bytes(cleanable) : QString();
    const int sizeWidth = size.isEmpty() ? 0 : fm.horizontalAdvance(size);
    const int nameWidth = qMax(0, inner.width() - sizeWidth - (sizeWidth ? Margin : 0));

    painter->setFont(opt.font);
    painter->setPen(textColor);
    painter->drawText(QRect(inner.left(), inner.top(), nameWidth, inner.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(opt.text, Qt::ElideRight, nameWidth));
    if (!size.isEmpty()) {
        painter->setPen(warning ? WarningColor : blend(textColor, background, 0.4));
        painter->drawText(inner, Qt::AlignRight | Qt::AlignVCenter, size);
    }
    painter->restore();
}
