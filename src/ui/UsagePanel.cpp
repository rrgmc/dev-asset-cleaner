#include "UsagePanel.h"

#include "Format.h"
#include "Icons.h"

#include <QEvent>
#include <QPainter>

namespace {
constexpr int HPadding = 10;
constexpr int VPadding = 3;
constexpr int ColumnGap = 22;
constexpr int NumberGap = 6;  // between the size and "to clean"
constexpr int LineGap = 1;
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

QString toCleanLabel()
{
    return UsagePanel::tr("to clean");
}
} // namespace

UsagePanel::UsagePanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("usagePanel"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void UsagePanel::setUsage(qint64 allBytes, qint64 selectionBytes, bool showSelection,
                          qint64 warningBytes, Severity warningLevel)
{
    m_warningLevel = warningLevel;
    m_all = allBytes;
    m_selection = selectionBytes;
    m_showSelection = showSelection;
    m_warning = warningBytes;

    QString tip = tr("All folders: %1").arg(cleanableText(allBytes));
    if (warningBytes != allBytes)
        tip += QLatin1Char('\n') + tr("Folders in the warnings: %1").arg(cleanableText(warningBytes));
    if (showSelection)
        tip += QLatin1Char('\n') + tr("Selected groups: %1").arg(cleanableText(selectionBytes));
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
    QList<Column> list{{tr("All folders"), m_all, CleanableColor}};
    if (m_warning != m_all) {
        list.append({tr("In warnings"), m_warning,
                     m_warningLevel == Severity::None ? QColor()
                                                      : Icons::severityTextColor(m_warningLevel)});
    }
    if (m_showSelection)
        list.append({tr("Selected"), m_selection, QColor()});
    return list;
}

QColor UsagePanel::sizeColor(const Column &column) const
{
    const QColor text = palette().color(QPalette::WindowText);
    if (column.cleanableBytes <= 0)
        return blend(text, palette().color(QPalette::Window), 0.45);
    return column.color.isValid() ? column.color : text;
}

QColor UsagePanel::sizeColor(const QString &caption) const
{
    for (const Column &c : columns()) {
        if (c.caption == caption)
            return sizeColor(c);
    }
    return {};
}

QStringList UsagePanel::captions() const
{
    QStringList list;
    for (const Column &c : columns())
        list.append(c.caption);
    return list;
}

int UsagePanel::columnWidth(const Column &column) const
{
    const QFontMetrics big(bigFont());
    const QFontMetrics small(smallFont());
    const int numbers = big.horizontalAdvance(Format::bytes(column.cleanableBytes)) + NumberGap
                        + small.horizontalAdvance(toCleanLabel());
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
    const int height = 2 * VPadding + small.height() + LineGap + big.height();
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
    const QColor rule = blend(text, window, 0.85);

    int x = HPadding;
    const QList<Column> cols = columns();
    for (int i = 0; i < cols.size(); ++i) {
        const Column &c = cols[i];
        if (i > 0) {
            // A thin rule between the columns.
            const int ruleX = x - ColumnGap / 2;
            painter.setPen(rule);
            painter.drawLine(QPointF(ruleX + 0.5, VPadding + 2),
                             QPointF(ruleX + 0.5, height() - VPadding - 2));
        }

        int y = VPadding;
        painter.setFont(small);
        painter.setPen(dim);
        painter.drawText(QPoint(x, y + smallMetrics.ascent()), c.caption);
        y += smallMetrics.height() + LineGap;

        const int baseline = y + bigMetrics.ascent();
        const QString size = Format::bytes(c.cleanableBytes);
        painter.setFont(big);
        painter.setPen(sizeColor(c));
        painter.drawText(QPoint(x, baseline), size);
        painter.setFont(small);
        painter.setPen(dim);
        painter.drawText(QPoint(x + bigMetrics.horizontalAdvance(size) + NumberGap, baseline),
                         toCleanLabel());

        x += columnWidth(c) + ColumnGap;
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
