#pragma once

#include <QColor>
#include <QTimer>
#include <QWidget>

class QPainter;
class QRectF;

// A busy indicator: a ring of dots that fade around the circle.
namespace Spinner {
// How often an animated spinner repaints.
constexpr int FrameMs = 80;
// Paints the spinner in `rect` at the current time, so every spinner turns in step.
void paint(QPainter *painter, const QRectF &rect, const QColor &color);
} // namespace Spinner

// A spinner as a widget. It animates only while it is running and visible.
class SpinnerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpinnerWidget(QWidget *parent = nullptr);

    // Shows the spinner and starts it, or stops it and hides it.
    void setRunning(bool running);
    bool isRunning() const { return m_running; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    bool m_running = false;
    QTimer m_timer;
};
