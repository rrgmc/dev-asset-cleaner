#include "Icons.h"

#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

#include <functional>

namespace Icons {

namespace {
// The bin's lid with its handle, as one shape.
QPainterPath lidPath(qreal s)
{
    QPainterPath lid;
    lid.addRoundedRect(QRectF(0.12 * s, 0.22 * s, 0.76 * s, 0.1 * s), 0.035 * s, 0.035 * s);
    QPainterPath handle;
    handle.addRoundedRect(QRectF(0.38 * s, 0.13 * s, 0.24 * s, 0.14 * s), 0.05 * s, 0.05 * s);
    QPainterPath grip;
    grip.addRoundedRect(QRectF(0.44 * s, 0.17 * s, 0.12 * s, 0.05 * s), 0.02 * s, 0.02 * s);
    return lid.united(handle).subtracted(grip);
}

// The bin's body: a tapered tub with rounded corners.
QPainterPath bodyPath(qreal s)
{
    const qreal top = 0.36 * s;
    const qreal bottom = 0.9 * s;
    const qreal r = 0.07 * s;
    QPainterPath path;
    path.moveTo(0.17 * s, top);
    path.lineTo(0.83 * s, top);
    path.lineTo(0.77 * s, bottom - r);
    path.quadTo(0.765 * s, bottom, 0.77 * s - r, bottom);
    path.lineTo(0.23 * s + r, bottom);
    path.quadTo(0.235 * s, bottom, 0.23 * s, bottom - r);
    path.closeSubpath();
    return path;
}

// Curly braces on the front of the bin, so that it reads as a bin for code rather than the
// system's trash.
QPainterPath bracesPath(qreal s)
{
    const auto brace = [s](qreal x, qreal dir) {
        // dir is 1 for "{" and -1 for "}"; x is where the brace's point sits.
        const qreal top = 0.45 * s;
        const qreal mid = 0.625 * s;
        const qreal bottom = 0.8 * s;
        const qreal w = 0.09 * s * dir;
        QPainterPath path;
        path.moveTo(x * s + w, top);
        path.cubicTo(x * s + w * 0.35, top, x * s + w * 0.45, top + 0.03 * s,
                     x * s + w * 0.45, top + 0.07 * s);
        path.lineTo(x * s + w * 0.45, mid - 0.05 * s);
        path.quadTo(x * s + w * 0.45, mid, x * s, mid);
        path.quadTo(x * s + w * 0.45, mid, x * s + w * 0.45, mid + 0.05 * s);
        path.lineTo(x * s + w * 0.45, bottom - 0.07 * s);
        path.cubicTo(x * s + w * 0.45, bottom - 0.03 * s, x * s + w * 0.35, bottom,
                     x * s + w, bottom);
        return path;
    };
    QPainterPathStroker stroker;
    stroker.setWidth(qMax(1.2, 0.065 * s));
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(brace(0.33, 1)).united(stroker.createStroke(brace(0.67, -1)))
        .simplified();
}

// A four-pointed sparkle centred at (x, y), as fractions of the icon size.
QPainterPath sparklePath(qreal s, qreal x, qreal y, qreal outer)
{
    const QPointF center(x * s, y * s);
    const qreal r = outer * s;
    const qreal inner = r * 0.28;
    QPainterPath path;
    for (int i = 0; i < 8; ++i) {
        const qreal radius = i % 2 == 0 ? r : inner;
        const qreal angle = qDegreesToRadians(-90.0 + i * 45.0);
        const QPointF p(center.x() + radius * qCos(angle), center.y() + radius * qSin(angle));
        if (i == 0)
            path.moveTo(p);
        else
            path.lineTo(p);
    }
    path.closeSubpath();
    return path;
}

QPainterPath sparkle(qreal s, qreal grow = 0)
{
    return sparklePath(s, 0.86, 0.11, 0.1 + grow);
}

// The dark outline and mark of the badges, which stand apart from the coral bin even at 16
// pixels.
const QColor BadgeInk(0x1B, 0x1B, 0x1B);

QColor badgeMarkColor(Severity severity)
{
    return severity == Severity::High ? QColor(Qt::white) : BadgeInk;
}

// The space to clean: a circle with an exclamation mark, at the bottom left.
void paintCleanBadge(QPainter &painter, qreal s, Severity severity)
{
    const QPointF center(0.25 * s, 0.75 * s);
    const qreal r = 0.23 * s;
    painter.setBrush(severityColor(severity));
    painter.setPen(QPen(BadgeInk, qMax(1.0, 0.04 * s)));
    painter.drawEllipse(center, r, r);
    painter.setPen(Qt::NoPen);
    painter.setBrush(badgeMarkColor(severity));
    const qreal w = 0.07 * s;
    painter.drawRoundedRect(QRectF(center.x() - w / 2, center.y() - 0.14 * s, w, 0.16 * s),
                            w / 2, w / 2);
    painter.drawEllipse(QPointF(center.x(), center.y() + 0.095 * s), w * 0.6, w * 0.6);
}

// Free space: a triangle pointing down, for space running out, at the bottom right.
void paintFreeBadge(QPainter &painter, qreal s, Severity severity)
{
    QPainterPath triangle;
    triangle.moveTo(0.52 * s, 0.53 * s);
    triangle.lineTo(0.99 * s, 0.53 * s);
    triangle.lineTo(0.755 * s, 0.98 * s);
    triangle.closeSubpath();
    QPen pen(BadgeInk, qMax(1.0, 0.04 * s));
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setBrush(severityColor(severity));
    painter.setPen(pen);
    painter.drawPath(triangle);
    // A bar across the top, as on a gauge that reads empty.
    painter.setPen(Qt::NoPen);
    painter.setBrush(badgeMarkColor(severity));
    const qreal w = 0.06 * s;
    painter.drawRoundedRect(QRectF(0.64 * s, 0.61 * s, 0.23 * s, w), w / 2, w / 2);
}

} // namespace

QColor severityColor(Severity severity)
{
    switch (severity) {
    case Severity::None:
        break;
    case Severity::Low:
        return QColor(0xFF, 0xC3, 0x00);
    case Severity::Medium:
        return QColor(0xFF, 0x7A, 0x00);
    case Severity::High:
        return QColor(0xE0, 0x20, 0x1B);
    }
    return {};
}

QColor severityTextColor(Severity severity)
{
    switch (severity) {
    case Severity::None:
        break;
    case Severity::Low:
        return QColor(0x9A, 0x6B, 0x00);
    case Severity::Medium:
        return QColor(0xD9, 0x48, 0x0F);
    case Severity::High:
        return QColor(0xC0, 0x10, 0x1A);
    }
    return {};
}

QPixmap badge(Severity severity, bool freeSpace, int size)
{
    const qreal ratio = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    const int pixels = qRound(size * ratio);
    QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    // The badges fill a quarter of the icon: draw that quarter at the full size.
    painter.scale(2, 2);
    painter.translate(freeSpace ? -0.5 * pixels : 0, -0.5 * pixels);
    if (freeSpace)
        paintFreeBadge(painter, pixels, severity);
    else
        paintCleanBadge(painter, pixels, severity);
    painter.end();
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}

QImage render(int size, bool mono, Severity clean, Severity free)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    // The lid gives way round the sparkle, so the two stay apart in the template image too.
    const QPainterPath lid = lidPath(size).subtracted(sparkle(size, 0.04));
    if (mono) {
        // Template images use alpha only: the braces are cut out of the body.
        painter.fillPath(lid, Qt::black);
        painter.fillPath(bodyPath(size).subtracted(bracesPath(size)), Qt::black);
        painter.fillPath(sparkle(size), Qt::black);
    } else {
        painter.fillPath(lid, QColor(0x8C, 0x3A, 0x28));
        painter.fillPath(bodyPath(size), QColor(0xE7, 0x6F, 0x51));
        painter.fillPath(bracesPath(size), QColor(0xFF, 0xF4, 0xEC));
        painter.fillPath(sparkle(size), QColor(0xF4, 0xC9, 0x5D));
    }
    if (clean != Severity::None)
        paintCleanBadge(painter, size, clean);
    if (free != Severity::None)
        paintFreeBadge(painter, size, free);
    painter.end();
    return image;
}

namespace {

QIcon build(bool mono, Severity clean = Severity::None, Severity free = Severity::None)
{
    QIcon icon;
    for (int size : {16, 22, 32, 44, 64, 128, 256})
        icon.addPixmap(QPixmap::fromImage(render(size, mono, clean, free)));
    return icon;
}
} // namespace

QIcon appIcon()
{
    static const QIcon icon = build(false);
    return icon;
}

QIcon trayIcon(Severity clean, Severity free)
{
    if (clean != Severity::None || free != Severity::None) {
        // In colour: a template image would lose the badges' colours.
        static QHash<int, QIcon> icons;
        const int key = static_cast<int>(clean) * 4 + static_cast<int>(free);
        auto it = icons.find(key);
        if (it == icons.end())
            it = icons.insert(key, build(false, clean, free));
        return *it;
    }
#ifdef Q_OS_MACOS
    static const QIcon icon = [] {
        QIcon i = build(true);
        i.setIsMask(true);
        return i;
    }();
    return icon;
#else
    return appIcon();
#endif
}

namespace {

// Line drawings on a 16 by 16 grid, in the palette's text colour, so that they follow dark mode
// and the disabled state.
class LineIconEngine : public QIconEngine
{
public:
    explicit LineIconEngine(std::function<void(QPainter *)> draw) : m_draw(std::move(draw)) {}

    QIconEngine *clone() const override { return new LineIconEngine(m_draw); }
    QString key() const override { return QStringLiteral("dev-asset-cleaner.tool"); }
    bool isNull() override { return false; }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QPalette palette = QGuiApplication::palette();
        QColor color = palette.color(QPalette::Active, QPalette::WindowText);
        if (mode == QIcon::Disabled)
            color = palette.color(QPalette::Disabled, QPalette::WindowText);
        else if (mode == QIcon::Selected)
            color = palette.color(QPalette::Active, QPalette::HighlightedText);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.topLeft());
        painter->scale(rect.width() / 16.0, rect.height() / 16.0);
        QPen pen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        m_draw(painter);
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state,
                         qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return pixmap;
    }

private:
    std::function<void(QPainter *)> m_draw;
};

#ifdef Q_OS_MACOS
void drawToolIcon(QPainter *p, QIcon::ThemeIcon icon)
{
    switch (icon) {
    case QIcon::ThemeIcon::FolderNew: {
        QPainterPath folder;
        folder.moveTo(9, 13.5);
        folder.lineTo(1.5, 13.5);
        folder.lineTo(1.5, 3);
        folder.lineTo(5.5, 3);
        folder.lineTo(7, 4.5);
        folder.lineTo(13.5, 4.5);
        folder.lineTo(13.5, 8);
        p->drawPath(folder);
        p->drawLine(QPointF(12.5, 9.5), QPointF(12.5, 15));
        p->drawLine(QPointF(9.75, 12.25), QPointF(15.25, 12.25));
        break;
    }
    case QIcon::ThemeIcon::DocumentProperties: {
        // A pencil, for editing.
        QPainterPath pencil;
        pencil.moveTo(2, 14);
        pencil.lineTo(2.8, 10.6);
        pencil.lineTo(10.8, 2.6);
        pencil.lineTo(13.4, 5.2);
        pencil.lineTo(5.4, 13.2);
        pencil.closeSubpath();
        p->drawPath(pencil);
        p->drawLine(QPointF(9.2, 4.2), QPointF(11.8, 6.8));
        break;
    }
    case QIcon::ThemeIcon::EditCopy: {
        // Two overlapping sheets; the back one shows only where the front one does not cover it.
        p->drawRoundedRect(QRectF(5.5, 5.5, 8.5, 9), 1.2, 1.2);
        QPainterPath back;
        back.moveTo(3.5, 10.5);
        back.lineTo(2, 10.5);
        back.lineTo(2, 1.5);
        back.lineTo(10.5, 1.5);
        back.lineTo(10.5, 3.5);
        p->drawPath(back);
        break;
    }
    case QIcon::ThemeIcon::ListRemove:
        p->drawEllipse(QPointF(8, 8), 6.25, 6.25);
        p->drawLine(QPointF(5, 8), QPointF(11, 8));
        break;
    case QIcon::ThemeIcon::SyncSynchronizing: {
        // Two arrows chasing each other round a circle.
        const QRectF circle(2.5, 2.5, 11, 11);
        p->drawArc(circle, 20 * 16, 140 * 16);
        p->drawArc(circle, 200 * 16, 140 * 16);
        const auto arrowHead = [p](qreal angle) {
            const qreal a = qDegreesToRadians(angle);
            const QPointF tip(8 + 5.5 * qCos(a), 8 - 5.5 * qSin(a));
            const QPointF along(qSin(a), qCos(a));  // the direction of travel
            const QPointF out(qCos(a), -qSin(a));
            p->drawPolyline(QList<QPointF>{tip - 2.4 * along + 2.0 * out, tip,
                                           tip - 2.4 * along - 2.0 * out});
        };
        arrowHead(20);
        arrowHead(200);
        break;
    }
    case QIcon::ThemeIcon::FormatJustifyLeft:
        p->drawLine(QPointF(2, 3.5), QPointF(14, 3.5));
        p->drawLine(QPointF(2, 6.5), QPointF(10, 6.5));
        p->drawLine(QPointF(2, 9.5), QPointF(14, 9.5));
        p->drawLine(QPointF(2, 12.5), QPointF(10, 12.5));
        break;
    case QIcon::ThemeIcon::DocumentPageSetup: {
        // A gear, for settings.
        QPainterPath gear;
        constexpr int Teeth = 8;
        for (int i = 0; i < Teeth * 4; ++i) {
            const qreal r = (i % 4 == 0 || i % 4 == 1) ? 6.5 : 4.9;
            const qreal a = qDegreesToRadians((i - 0.5) * 360.0 / (Teeth * 4) + 5.625);
            const QPointF pt(8 + r * qCos(a), 8 + r * qSin(a));
            if (i == 0)
                gear.moveTo(pt);
            else
                gear.lineTo(pt);
        }
        gear.closeSubpath();
        p->drawPath(gear);
        p->drawEllipse(QPointF(8, 8), 2, 2);
        break;
    }
    case QIcon::ThemeIcon::ViewRefresh: {
        // A circular arrow, for rescanning.
        const QRectF circle(2.5, 2.5, 11, 11);
        p->drawArc(circle, 60 * 16, 300 * 16);
        const qreal a = qDegreesToRadians(60.0);
        const QPointF tip(8 + 5.5 * qCos(a), 8 - 5.5 * qSin(a));
        p->drawPolyline(QList<QPointF>{tip + QPointF(-2.8, -0.6), tip, tip + QPointF(0.4, 2.8)});
        break;
    }
    case QIcon::ThemeIcon::EditClear: {
        // A broom, for cleaning one folder.
        p->drawLine(QPointF(13.5, 1.5), QPointF(8.2, 6.8));
        QPainterPath head;
        head.moveTo(7, 5.6);
        head.lineTo(10.4, 9);
        head.lineTo(6.5, 14.5);
        head.lineTo(1.5, 9.5);
        head.closeSubpath();
        p->drawPath(head);
        p->drawLine(QPointF(4, 12), QPointF(6.8, 9.2));
        break;
    }
    case QIcon::ThemeIcon::EditDelete: {
        // A bin, for cleaning every folder.
        p->drawLine(QPointF(2, 4), QPointF(14, 4));
        p->drawPolyline(QList<QPointF>{{6, 4}, {6, 2}, {10, 2}, {10, 4}});
        QPainterPath bin;
        bin.moveTo(3.5, 4);
        bin.lineTo(4.5, 14.5);
        bin.lineTo(11.5, 14.5);
        bin.lineTo(12.5, 4);
        p->drawPath(bin);
        p->drawLine(QPointF(6.7, 7), QPointF(6.9, 12));
        p->drawLine(QPointF(9.3, 7), QPointF(9.1, 12));
        break;
    }
    case QIcon::ThemeIcon::DriveHarddisk: {
        // A disk drive with a light and a slot, for free space.
        p->drawRoundedRect(QRectF(1.5, 4.5, 13, 7), 1.5, 1.5);
        p->drawLine(QPointF(4, 8), QPointF(9, 8));
        p->drawEllipse(QPointF(12, 8), 0.6, 0.6);
        break;
    }
    default:
        break;
    }
}
#endif

// An arrow pointing down at the right, shared by the sort icons.
void drawSortArrow(QPainter *p)
{
    p->drawLine(QPointF(12.5, 2.5), QPointF(12.5, 13.5));
    p->drawPolyline(QList<QPointF>{QPointF(10, 11), QPointF(12.5, 13.5), QPointF(15, 11)});
}

} // namespace

QIcon toolIcon(QIcon::ThemeIcon icon)
{
#ifdef Q_OS_MACOS
    return QIcon(new LineIconEngine([icon](QPainter *p) { drawToolIcon(p, icon); }));
#else
    return QIcon::fromTheme(icon);
#endif
}

QIcon sortGroupsIcon()
{
    // Three rows, like the group list, and the arrow.
    return QIcon(new LineIconEngine([](QPainter *p) {
        p->drawRoundedRect(QRectF(1.5, 2, 7.5, 3), 1, 1);
        p->drawRoundedRect(QRectF(1.5, 6.5, 7.5, 3), 1, 1);
        p->drawRoundedRect(QRectF(1.5, 11, 7.5, 3), 1, 1);
        drawSortArrow(p);
    }));
}

QIcon sortFoldersIcon()
{
    // A folder and the arrow.
    return QIcon(new LineIconEngine([](QPainter *p) {
        QPainterPath folder;
        folder.moveTo(1.5, 13);
        folder.lineTo(1.5, 3.5);
        folder.lineTo(4.5, 3.5);
        folder.lineTo(5.7, 5);
        folder.lineTo(9.5, 5);
        folder.lineTo(9.5, 13);
        folder.closeSubpath();
        p->drawPath(folder);
        drawSortArrow(p);
    }));
}

} // namespace Icons
