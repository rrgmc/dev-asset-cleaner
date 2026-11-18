#include "Spinner.h"

#include <QDateTime>
#include <QPainter>
#include <QtMath>

namespace {
constexpr int Dots = 8;
constexpr int TurnMs = Dots * Spinner::FrameMs * 1.5;
} // namespace

void Spinner::paint(QPainter *painter, const QRectF &rect, const QColor &color)
{
    const qreal side = qMin(rect.width(), rect.height());
    if (side <= 0)
        return;
    const QPointF center = rect.center();
    const qreal dot = side * 0.11;
    const qreal radius = side / 2 - dot;
    const int lead = static_cast<int>(QDateTime::currentMSecsSinceEpoch() % TurnMs) * Dots / TurnMs;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    for (int i = 0; i < Dots; ++i) {
        // The leading dot is solid; the ones behind it fade out.
        const int behind = (lead - i + Dots) % Dots;
        QColor c = color;
        c.setAlphaF(color.alphaF() * (1.0 - 0.8 * behind / (Dots - 1)));
        painter->setBrush(c);
        const qreal angle = 2 * M_PI * i / Dots - M_PI / 2;
        painter->drawEllipse(center + QPointF(qCos(angle), qSin(angle)) * radius, dot, dot);
    }
    painter->restore();
}

SpinnerWidget::SpinnerWidget(QWidget *parent)
    : QWidget(parent)
{
    m_timer.setInterval(Spinner::FrameMs);
    connect(&m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    setVisible(false);
}

void SpinnerWidget::setRunning(bool running)
{
    if (running == m_running)
        return;
    m_running = running;
    setVisible(running);
}

QSize SpinnerWidget::sizeHint() const
{
    const int side = fontMetrics().height();
    return QSize(side, side);
}

void SpinnerWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const int side = qMin(width(), height());
    const QRectF square((width() - side) / 2.0, (height() - side) / 2.0, side, side);
    Spinner::paint(&painter, square, palette().color(QPalette::WindowText));
}

void SpinnerWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_timer.start();
}

void SpinnerWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer.stop();
}
