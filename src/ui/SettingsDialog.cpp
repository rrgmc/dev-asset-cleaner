#include "SettingsDialog.h"

#include "CleanChoice.h"
#include "Format.h"
#include "Icons.h"
#include "LoginItem.h"
#include "SizeSpinBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

AlertEditor::AlertEditor(const QString &title, const Alert &alert, Alert::Direction direction,
                         QWidget *parent)
    : QGroupBox(title, parent)
    , m_direction(direction)
    , m_notify(new QComboBox(this))
    , m_ranges(new QLabel(this))
{
    const bool below = direction == Alert::Direction::Below;
    auto *grid = new QGridLayout(this);
    for (int i = 0; i < Alert::LevelCount; ++i) {
        const auto severity = static_cast<Severity>(i + 1);
        auto *swatch = new QLabel(this);
        swatch->setPixmap(Icons::badge(severity, below, 16));
        auto *spin = new SizeSpinBox(this);
        spin->setBytes(alert.limits[i]);
        m_limits[i] = spin;
        //: %1 is a warning level: Low, Medium or High.
        auto *label = new QLabel(below ? tr("%1 below:").arg(severityName(severity))
                                       : tr("%1 above:").arg(severityName(severity)),
                                 this);
        label->setBuddy(spin);
        grid->addWidget(swatch, i, 0);
        grid->addWidget(label, i, 1);
        grid->addWidget(spin, i, 2);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] {
            updateRanges();
            emit edited();
        });
    }

    m_notify->addItem(tr("Never"), static_cast<int>(Severity::None));
    for (int i = 1; i <= Alert::LevelCount; ++i) {
        const auto severity = static_cast<Severity>(i);
        m_notify->addItem(QIcon(Icons::badge(severity, below, 16)), severityName(severity), i);
    }
    m_notify->setCurrentIndex(m_notify->findData(static_cast<int>(alert.notifyFrom)));
    m_notify->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    auto *notifyLabel = new QLabel(tr("&Notify from:"), this);
    notifyLabel->setBuddy(m_notify);
    grid->addWidget(notifyLabel, Alert::LevelCount, 1);
    grid->addWidget(m_notify, Alert::LevelCount, 2, Qt::AlignLeft);

    m_ranges->setWordWrap(true);
    grid->addWidget(m_ranges, Alert::LevelCount + 1, 0, 1, 4);
    grid->setColumnStretch(3, 1);
    updateRanges();
}

Alert AlertEditor::alert() const
{
    Alert a;
    for (int i = 0; i < Alert::LevelCount; ++i)
        a.limits[i] = m_limits[i]->bytes();
    a.notifyFrom = static_cast<Severity>(m_notify->currentData().toInt());
    return a;
}

QString AlertEditor::problem() const
{
    return alert().orderProblem(m_direction);
}

void AlertEditor::updateRanges()
{
    const Alert a = alert();
    const QString problem = a.orderProblem(m_direction);
    QPalette palette = this->palette();
    if (!problem.isEmpty()) {
        palette.setColor(QPalette::WindowText, Icons::severityTextColor(Severity::High));
        m_ranges->setPalette(palette);
        m_ranges->setText(problem);
        return;
    }
    m_ranges->setPalette(palette);

    // Each level runs from its own limit to the next level's limit that is on.
    QStringList parts;
    for (int i = 0; i < Alert::LevelCount; ++i) {
        if (a.limits[i] <= 0)
            continue;
        qint64 end = 0;
        for (int j = i + 1; j < Alert::LevelCount && end == 0; ++j)
            end = a.limits[j];
        const QString name = severityName(static_cast<Severity>(i + 1));
        const QString from = Format::bytes(a.limits[i]);
        if (m_direction == Alert::Direction::Above) {
            parts.append(end > 0 ? tr("%1 from %2 to %3").arg(name, from, Format::bytes(end))
                                 : tr("%1 above %2").arg(name, from));
        } else {
            parts.append(end > 0 ? tr("%1 from %2 down to %3").arg(name, from, Format::bytes(end))
                                 : tr("%1 below %2").arg(name, from));
        }
    }
    m_ranges->setText(parts.isEmpty() ? tr("This warning is off.")
                                      : parts.join(QStringLiteral(", ")) + QLatin1Char('.'));
}

SettingsDialog::SettingsDialog(const Settings &settings, bool trayAvailable, QWidget *parent)
    : QDialog(parent)
    , m_original(settings)
    , m_closeToTray(new QCheckBox(tr("&Closing the window keeps the app running in the tray"), this))
    , m_startAtSignIn(new QCheckBox(tr("&Start when I sign in"), this))
    , m_deleteDirectly(new QCheckBox(
          tr("Cleaning &deletes directly instead of moving to the %1").arg(CleanChoice::trashName()),
          this))
    , m_rescan(new QSpinBox(this))
    , m_cleanAlert(new AlertEditor(tr("Space to clean in all folders"), settings.cleanAlert,
                                   Alert::Direction::Above, this))
    , m_freeAlert(new AlertEditor(tr("Free space on each disk"), settings.freeAlert,
                                  Alert::Direction::Below, this))
{
    setWindowTitle(tr("Settings"));
    m_cleanAlert->setObjectName(QStringLiteral("cleanAlert"));
    m_freeAlert->setObjectName(QStringLiteral("freeAlert"));

    m_rescan->setRange(0, 24 * 60);
    m_rescan->setSuffix(tr(" min"));
    m_rescan->setSpecialValueText(tr("Off"));
    m_rescan->setValue(settings.rescanMinutes);

    auto *form = new QFormLayout;
    form->addRow(tr("&Rescan every:"), m_rescan);

    auto *alerts = new QHBoxLayout;
    alerts->addWidget(m_cleanAlert);
    alerts->addWidget(m_freeAlert);
    auto *hint = new QLabel(
        tr("Each warning shows as a badge on the tray icon in its level's colour: a circle on the "
           "left for the space to clean, a triangle on the right for free space. Each folder can "
           "also have its own limit, which counts as Low. The Free space window picks the disks "
           "that warn."),
        this);
    hint->setWordWrap(true);

    m_deleteDirectly->setChecked(settings.deleteDirectly);
    auto *trashHint = new QLabel(
        tr("Otherwise cleaning moves items to the %1, and holding Shift while you click Clean "
           "deletes them directly. Items already in the %1 are always deleted.")
            .arg(CleanChoice::trashName()),
        this);
    trashHint->setWordWrap(true);

    auto *options = new QFormLayout;
    options->addRow(QString(), m_deleteDirectly);
    options->addRow(QString(), trashHint);

    m_closeToTray->setChecked(settings.closeToTray && trayAvailable);
    m_closeToTray->setEnabled(trayAvailable);
    options->addRow(QString(), m_closeToTray);

    // The OS holds this state, so read it each time rather than storing it in Settings.
    if (LoginItem::isSupported()) {
        m_startAtSignIn->setChecked(LoginItem::isEnabled());
        options->addRow(QString(), m_startAtSignIn);
    } else {
        m_startAtSignIn->hide();
    }

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_cleanAlert, &AlertEditor::edited, this, &SettingsDialog::updateOk);
    connect(m_freeAlert, &AlertEditor::edited, this, &SettingsDialog::updateOk);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(alerts);
    layout->addWidget(hint);
    layout->addLayout(options);
    layout->addWidget(m_buttons);
    updateOk();
}

void SettingsDialog::updateOk()
{
    m_buttons->button(QDialogButtonBox::Ok)
        ->setEnabled(m_cleanAlert->problem().isEmpty() && m_freeAlert->problem().isEmpty());
}

Settings SettingsDialog::settings() const
{
    Settings s = m_original;
    // Without a tray the checkbox is disabled; keep the stored choice for when one appears.
    s.closeToTray = m_closeToTray->isEnabled() ? m_closeToTray->isChecked() : m_original.closeToTray;
    s.rescanMinutes = m_rescan->value();
    s.cleanAlert = m_cleanAlert->alert();
    s.freeAlert = m_freeAlert->alert();
    s.deleteDirectly = m_deleteDirectly->isChecked();
    return s;
}

bool SettingsDialog::startAtSignIn() const
{
    return m_startAtSignIn->isChecked();
}
