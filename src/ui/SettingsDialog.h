#pragma once

#include "model/Store.h"

#include <QDialog>
#include <QGroupBox>

#include <array>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QSpinBox;
class SizeSpinBox;

// The three levels of one warning and the level it notifies from, with a line that says where
// each level starts and ends.
class AlertEditor : public QGroupBox
{
    Q_OBJECT

public:
    AlertEditor(const QString &title, const Alert &alert, Alert::Direction direction,
                QWidget *parent = nullptr);

    Alert alert() const;
    // Empty when the levels are in order.
    QString problem() const;

    SizeSpinBox *limitBox(int index) const { return m_limits[index]; }
    QComboBox *notifyBox() const { return m_notify; }

signals:
    void edited();

private:
    void updateRanges();

    Alert::Direction m_direction;
    std::array<SizeSpinBox *, Alert::LevelCount> m_limits{};
    QComboBox *m_notify;
    QLabel *m_ranges;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(const Settings &settings, bool trayAvailable, QWidget *parent = nullptr);

    Settings settings() const;

    // Whether the app should start at sign-in. Only meaningful when LoginItem::isSupported().
    bool startAtSignIn() const;

    AlertEditor *cleanAlertEditor() const { return m_cleanAlert; }
    AlertEditor *freeAlertEditor() const { return m_freeAlert; }

private:
    void updateOk();

    Settings m_original;
    QCheckBox *m_closeToTray;
    QCheckBox *m_startAtSignIn;
    QCheckBox *m_deleteDirectly;
    QSpinBox *m_rescan;
    AlertEditor *m_cleanAlert;
    AlertEditor *m_freeAlert;
    QDialogButtonBox *m_buttons = nullptr;
};
