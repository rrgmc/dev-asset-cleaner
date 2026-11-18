#include "SettingsDialog.h"

#include "CleanChoice.h"
#include "LoginItem.h"
#include "SizeSpinBox.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const Settings &settings, bool trayAvailable, QWidget *parent)
    : QDialog(parent)
    , m_original(settings)
    , m_closeToTray(new QCheckBox(tr("&Closing the window keeps the app running in the tray"), this))
    , m_startAtSignIn(new QCheckBox(tr("&Start when I sign in"), this))
    , m_deleteDirectly(new QCheckBox(
          tr("Cleaning &deletes directly instead of moving to the %1").arg(CleanChoice::trashName()),
          this))
    , m_rescan(new QSpinBox(this))
    , m_freeWarn(new SizeSpinBox(this))
    , m_totalWarn(new SizeSpinBox(this))
{
    setWindowTitle(tr("Settings"));

    m_rescan->setRange(0, 24 * 60);
    m_rescan->setSuffix(tr(" min"));
    m_rescan->setSpecialValueText(tr("Off"));
    m_rescan->setValue(settings.rescanMinutes);
    m_freeWarn->setBytes(settings.freeWarnBytes);
    m_totalWarn->setBytes(settings.totalWarnBytes);

    auto *form = new QFormLayout;
    form->addRow(tr("&Rescan every:"), m_rescan);
    form->addRow(tr("Warn when &free space is below:"), m_freeWarn);
    form->addRow(tr("Warn when all folders &have more to clean than:"), m_totalWarn);
    auto *hint = new QLabel(tr("Warnings show as a badge on the tray icon. Each folder can also "
                               "have its own limit."),
                            this);
    hint->setWordWrap(true);
    form->addRow(QString(), hint);

    m_deleteDirectly->setChecked(settings.deleteDirectly);
    form->addRow(QString(), m_deleteDirectly);
    auto *trashHint = new QLabel(
        tr("Otherwise cleaning moves items to the %1, and holding Shift while you click Clean "
           "deletes them directly. Items already in the %1 are always deleted.")
            .arg(CleanChoice::trashName()),
        this);
    trashHint->setWordWrap(true);
    form->addRow(QString(), trashHint);

    m_closeToTray->setChecked(settings.closeToTray && trayAvailable);
    m_closeToTray->setEnabled(trayAvailable);
    form->addRow(QString(), m_closeToTray);

    // The OS holds this state, so read it each time rather than storing it in Settings.
    if (LoginItem::isSupported()) {
        m_startAtSignIn->setChecked(LoginItem::isEnabled());
        form->addRow(QString(), m_startAtSignIn);
    } else {
        m_startAtSignIn->hide();
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

Settings SettingsDialog::settings() const
{
    Settings s = m_original;
    // Without a tray the checkbox is disabled; keep the stored choice for when one appears.
    s.closeToTray = m_closeToTray->isEnabled() ? m_closeToTray->isChecked() : m_original.closeToTray;
    s.rescanMinutes = m_rescan->value();
    s.freeWarnBytes = m_freeWarn->bytes();
    s.totalWarnBytes = m_totalWarn->bytes();
    s.deleteDirectly = m_deleteDirectly->isChecked();
    return s;
}

bool SettingsDialog::startAtSignIn() const
{
    return m_startAtSignIn->isChecked();
}
