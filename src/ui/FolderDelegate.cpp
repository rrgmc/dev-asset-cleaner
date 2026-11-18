#include "FolderDelegate.h"

#include "FolderModel.h"
#include "Format.h"
#include "Spinner.h"

#include <QAbstractItemView>
#include <QCursor>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int Margin = 8;
constexpr int VPadding = 6;
constexpr int LineGap = 2;
constexpr int ButtonHPadding = 10;
constexpr int ButtonVPadding = 3;
constexpr int ButtonRadius = 4;
constexpr int SizeColumnWidthChars = 18;

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}

const QColor WarningColor(0xD9, 0x48, 0x0F);
} // namespace

QString FolderDelegate::buttonLabel()
{
    return tr("Clean");
}

QRect FolderDelegate::buttonRect(const QRect &rect, const QFontMetrics &fm)
{
    const int width = fm.horizontalAdvance(buttonLabel()) + 2 * ButtonHPadding;
    const int height = fm.height() + 2 * ButtonVPadding;
    return QRect(rect.right() - Margin - width + 1, rect.top() + (rect.height() - height) / 2,
                 width, height);
}

QSize FolderDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    const QFontMetrics fm(option.font);
    return QSize(300, 2 * fm.height() + LineGap + 2 * VPadding);
}

void FolderDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // The row paints its own selection, as in favdir: a light tint of the accent colour that
    // keeps the normal text colour readable in every style.
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hoveredRow = opt.state & QStyle::State_MouseOver;
    const QPalette::ColorGroup group = opt.state & QStyle::State_Active ? QPalette::Active
                                                                          : QPalette::Inactive;
    const QColor base = opt.palette.color(group, QPalette::Base);
    const QColor accent = opt.palette.color(QPalette::Active, QPalette::Highlight);
    const QColor textColor = opt.palette.color(group, QPalette::Text);
    QColor background = base;
    if (selected)
        background = blend(base, accent, group == QPalette::Active ? 0.22 : 0.12);
    else if (hoveredRow)
        background = blend(base, accent, 0.07);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (selected || hoveredRow) {
        const QRectF r = QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5);
        QPainterPath path;
        path.addRoundedRect(r, 4, 4);
        painter->fillPath(path, background);
        if (selected) {
            painter->setPen(QPen(accent, 1.5));
            painter->drawPath(path);
        }
    }

    const QFontMetrics fm(opt.font);
    QFont boldFont = opt.font;
    boldFont.setBold(true);
    const QFontMetrics boldFm(boldFont);
    const QColor dim = blend(textColor, background, 0.4);

    const QRect button = buttonRect(opt.rect, fm);
    const int sizeWidth = fm.averageCharWidth() * SizeColumnWidthChars;
    const int sizeRight = button.left() - Margin;
    const int sizeLeft = sizeRight - sizeWidth;
    const int textLeft = opt.rect.left() + Margin;
    const int textWidth = qMax(0, sizeLeft - Margin - textLeft);
    const int line1 = opt.rect.top() + VPadding;
    const int line2 = line1 + boldFm.height() + LineGap;

    // Name, then the types in the remaining space.
    const QString name = boldFm.elidedText(opt.text, Qt::ElideRight, textWidth);
    painter->setFont(boldFont);
    painter->setPen(textColor);
    painter->drawText(QRect(textLeft, line1, textWidth, boldFm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, name);
    const int nameWidth = boldFm.horizontalAdvance(name);
    const int typesLeft = textLeft + nameWidth + fm.averageCharWidth() * 2;
    const int typesWidth = textLeft + textWidth - typesLeft;
    if (typesWidth > fm.averageCharWidth() * 4) {
        painter->setFont(opt.font);
        painter->setPen(dim);
        painter->drawText(QRect(typesLeft, line1, typesWidth, boldFm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(index.data(FolderModel::TypesRole).toString(),
                                        Qt::ElideRight, typesWidth));
    }

    painter->setFont(opt.font);
    painter->setPen(dim);
    painter->drawText(QRect(textLeft, line2, textWidth, fm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(index.data(FolderModel::PathRole).toString(), Qt::ElideMiddle,
                                    textWidth));

    // Size column: what cleaning frees, then the folder's state.
    const bool scanned = index.data(FolderModel::ScannedRole).toBool();
    const bool warning = index.data(FolderModel::WarningRole).toBool();
    const qint64 cleanable = index.data(FolderModel::CleanableRole).toLongLong();
    QString headline;
    if (scanned)
        headline = cleanable > 0 ? tr("%1 to clean").arg(Format::bytes(cleanable))
                                 : tr("Nothing to clean");
    painter->setFont(boldFont);
    painter->setPen(warning ? WarningColor : textColor);
    painter->drawText(QRect(sizeLeft, line1, sizeWidth, boldFm.height()),
                      Qt::AlignRight | Qt::AlignVCenter,
                      boldFm.elidedText(headline, Qt::ElideRight, sizeWidth));
    painter->setFont(opt.font);
    painter->setPen(warning ? WarningColor : dim);
    const QString status = fm.elidedText(index.data(FolderModel::StatusRole).toString(),
                                         Qt::ElideRight, sizeWidth);
    painter->drawText(QRect(sizeLeft, line2, sizeWidth, fm.height()),
                      Qt::AlignRight | Qt::AlignVCenter, status);
    // A spinner left of "Scanning…" or "Cleaning…".
    if (index.data(FolderModel::BusyRole).toBool()) {
        const int side = fm.ascent();
        const int right = sizeRight - fm.horizontalAdvance(status) - fm.averageCharWidth();
        Spinner::paint(painter,
                       QRectF(right - side, line2 + (fm.height() - side) / 2.0, side, side), dim);
    }

    // The Clean button.
    const bool enabled = index.data(FolderModel::CanCleanRole).toBool();
    bool hovered = false;
    if (enabled && hoveredRow && opt.widget) {
        const auto *view = qobject_cast<const QAbstractItemView *>(opt.widget);
        const QWidget *viewport = view ? view->viewport() : opt.widget;
        hovered = button.contains(viewport->mapFromGlobal(QCursor::pos()));
    }
    const QPalette::ColorGroup buttonGroup = enabled ? group : QPalette::Disabled;
    const QColor fill = opt.palette.color(buttonGroup, QPalette::Button);
    const QColor buttonText = opt.palette.color(buttonGroup, QPalette::ButtonText);
    QPainterPath path;
    path.addRoundedRect(QRectF(button).adjusted(0.5, 0.5, -0.5, -0.5), ButtonRadius, ButtonRadius);
    painter->fillPath(path, hovered ? blend(fill, accent, 0.35) : fill);
    painter->setPen(blend(buttonText, fill, enabled ? 0.7 : 0.85));
    painter->drawPath(path);
    painter->setPen(enabled ? buttonText : blend(buttonText, fill, 0.5));
    painter->drawText(button, Qt::AlignCenter, buttonLabel());

    painter->restore();
}
