#include "TrayIcon.h"

#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

class TrayIcon::Private
{
public:
    QSystemTrayIcon *tray = nullptr;
};

bool TrayIcon::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

TrayIcon::TrayIcon(const QIcon &icon, const QString &toolTip, QMenu *menu, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->tray = new QSystemTrayIcon(icon, this);
    d->tray->setToolTip(toolTip);
    d->tray->setContextMenu(menu);
    connect(d->tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger)
                    emit triggered();
            });
    d->tray->show();
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::setIcon(const QIcon &icon, const QIcon &badges)
{
    if (badges.isNull()) {
        d->tray->setIcon(icon);
        return;
    }
    QIcon combined;
    for (const QSize &size : icon.availableSizes()) {
        QPixmap pixmap = icon.pixmap(size, 1.0);
        QPainter painter(&pixmap);
        painter.drawPixmap(pixmap.rect(), badges.pixmap(size, 1.0));
        painter.end();
        combined.addPixmap(pixmap);
    }
    d->tray->setIcon(combined);
}

void TrayIcon::setToolTip(const QString &toolTip)
{
    d->tray->setToolTip(toolTip);
}

void TrayIcon::showWarning(const QString &title, const QString &text)
{
    d->tray->showMessage(title, text, QSystemTrayIcon::Warning);
}

void TrayIcon::setDockIconVisible(bool)
{
}
