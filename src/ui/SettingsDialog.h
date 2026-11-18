#pragma once

#include "model/Store.h"

#include <QDialog>

class QCheckBox;
class QSpinBox;
class SizeSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(const Settings &settings, bool trayAvailable, QWidget *parent = nullptr);

    Settings settings() const;

    // Whether the app should start at sign-in. Only meaningful when LoginItem::isSupported().
    bool startAtSignIn() const;

private:
    Settings m_original;
    QCheckBox *m_closeToTray;
    QCheckBox *m_startAtSignIn;
    QCheckBox *m_deleteDirectly;
    QSpinBox *m_rescan;
    SizeSpinBox *m_freeWarn;
    SizeSpinBox *m_totalWarn;
};
