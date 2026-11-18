#include "UsagePanel.h"

#include "Format.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int HPadding = 10;
constexpr int VPadding = 3;
constexpr int ColumnGap = 22;
constexpr int NumberGap = 8;  // between the total and the part to clean
constexpr int LineGap = 1;
constexpr int BarHeight = 3;
constexpr qreal BigScale = 1.6;

// The coral of the app icon: what cleaning would free.
const QColor CleanableColor(0xE7, 0x6F, 0x51);

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}

QString cleanableText(qint64 bytes)
{
    return UsagePanel::tr("%1 to clean").arg(Format::bytes(bytes));
}
} // namespace

UsagePanel::UsagePanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("usagePanel"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void UsagePanel::setUsage(const Usage &all, const Usage &selection, bool showSelection)
{
    m_all = all;
    m_selection = selection;
    m_showSelection = showSelection;

    QString tip = tr("All folders: %1, %2")
                      .arg(Format::bytes(all.totalBytes), cleanableText(all.cleanableBytes));
    if (showSelection)
        tip += QLatin1Char('\n')
               + tr("Selected groups: %1, %2")
                     .arg(Format::bytes(selection.totalBytes),
                          cleanableText(selection.cleanableBytes));
    setToolTip(tip);

    updateGeometry();
    update();
}

QFont UsagePanel::bigFont() const
{
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * BigScale);
    f.setWeight(QFont::DemiBold);
    return f;
}

QFont UsagePanel::smallFont() const
{
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 0.9);
    return f;
}

QList<UsagePanel::Column> UsagePanel::columns() const
{
    QList<Column> list{{tr("All folders"), m_all, palette().color(QPalette::WindowText)}};
    if (m_showSelection)
        list.append({tr("Selected"), m_selection, palette().color(QPalette::Active, QPalette::Highlight)});
    return list;
}

int UsagePanel::columnWidth(const Column &column) const
{
    const QFontMetrics big(bigFont());
    const QFontMetrics small(smallFont());
    const int numbers = big.horizontalAdvance(Format::bytes(column.usage.totalBytes)) + NumberGap
                        + small.horizontalAdvance(cleanableText(column.usage.cleanableBytes));
    return std::max(numbers, small.horizontalAdvance(column.caption));
}

QSize UsagePanel::sizeHint() const
{
    const QFontMetrics big(bigFont());
    const QFontMetrics small(smallFont());
    int width = 2 * HPadding;
    const QList<Column> cols = columns();
    for (const Column &c : cols)
        width += columnWidth(c);
    width += ColumnGap * (int(cols.size()) - 1);
    const int height = 2 * VPadding + small.height() + LineGap + big.height() + LineGap + BarHeight;
    return QSize(width, height);
}

void UsagePanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QFont big = bigFont();
    const QFont small = smallFont();
    const QFontMetrics bigMetrics(big);
    const QFontMetrics smallMetrics(small);
    const QColor text = palette().color(QPalette::WindowText);
    const QColor window = palette().color(QPalette::Window);
    const QColor dim = blend(text, window, 0.45);
    const QColor track = blend(text, window, 0.85);

    int x = HPadding;
    const QList<Column> cols = columns();
    for (int i = 0; i < cols.size(); ++i) {
        const Column &c = cols[i];
        const int width = columnWidth(c);
        if (i > 0) {
            // A thin rule between the columns.
            const int ruleX = x - ColumnGap / 2;
            painter.setPen(track);
            painter.drawLine(QPointF(ruleX + 0.5, VPadding + 2),
                             QPointF(ruleX + 0.5, height() - VPadding - 2));
        }

        int y = VPadding;
        painter.setFont(small);
        painter.setPen(dim);
        painter.drawText(QPoint(x, y + smallMetrics.ascent()), c.caption);
        y += smallMetrics.height() + LineGap;

        const int baseline = y + bigMetrics.ascent();
        const QString total = Format::bytes(c.usage.totalBytes);
        painter.setFont(big);
        painter.setPen(c.color);
        painter.drawText(QPoint(x, baseline), total);
        painter.setFont(small);
        painter.setPen(c.usage.cleanableBytes > 0 ? CleanableColor : dim);
        painter.drawText(QPoint(x + bigMetrics.horizontalAdvance(total) + NumberGap, baseline),
                         cleanableText(c.usage.cleanableBytes));
        y += bigMetrics.height() + LineGap;

        // The bar: the share of the total that cleaning would free.
        const QRectF bar(x, y, width, BarHeight);
        QPainterPath trackPath;
        trackPath.addRoundedRect(bar, BarHeight / 2.0, BarHeight / 2.0);
        painter.fillPath(trackPath, track);
        if (c.usage.totalBytes > 0 && c.usage.cleanableBytes > 0) {
            const qreal share = std::min(1.0, qreal(c.usage.cleanableBytes) / qreal(c.usage.totalBytes));
            QRectF filled = bar;
            filled.setWidth(std::max(qreal(BarHeight), bar.width() * share));
            QPainterPath fillPath;
            fillPath.addRoundedRect(filled, BarHeight / 2.0, BarHeight / 2.0);
            painter.fillPath(fillPath, CleanableColor);
        }

        x += width + ColumnGap;
    }
}

void UsagePanel::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}
