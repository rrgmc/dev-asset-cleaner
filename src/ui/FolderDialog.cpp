#include "FolderDialog.h"

#include "ExcludeDialog.h"
#include "ItemCheckStyle.h"
#include "SizeSpinBox.h"
#include "model/Store.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int TypeIdRole = Qt::UserRole + 1;
}

FolderDialog::FolderDialog(Store *store, const Folder &folder, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
    , m_name(new QLineEdit(folder.name, this))
    , m_path(new QLineEdit(QDir::toNativeSeparators(folder.path), this))
    , m_excludes(new QLabel(ExcludeDialog::summary(folder.excludes), this))
    , m_warn(new SizeSpinBox(this))
    , m_inWarnings(new QCheckBox(tr("&Include this folder in the warnings"), this))
{
    setWindowTitle(tr("Folder"));
    resize(520, folder.kind == Folder::Kind::Matching ? 580 : 320);

    auto *browse = new QPushButton(tr("&Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &FolderDialog::browse);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);

    // The kind is set when the folder is added and does not change.
    auto *kind = new QLabel(Folder::kindDescription(folder.kind), this);
    kind->setWordWrap(true);

    if (folder.kind == Folder::Kind::Matching) {
        m_types = new QListWidget(this);
        for (const CleanerType &t : store->types()) {
            auto *item = new QListWidgetItem(t.name, m_types);
            item->setData(TypeIdRole, t.id);
            item->setToolTip(t.description);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
            item->setCheckState(folder.typeIds.contains(t.id) ? Qt::Checked : Qt::Unchecked);
        }
        ItemCheckStyle::apply(m_types);
        connect(m_types, &QListWidget::itemChanged, this, &FolderDialog::updateOk);
    }

    m_warn->setBytes(folder.warnBytes);
    m_inWarnings->setChecked(folder.inWarnings);
    m_inWarnings->setToolTip(
        tr("Whether this folder's space to clean, and its own warning size, can make the tray "
           "icon warn and notify."));

    m_excludes->setWordWrap(true);
    m_excludes->setTextFormat(Qt::PlainText);
    auto *chooseExcludes = new QPushButton(tr("&Choose…"), this);
    connect(chooseExcludes, &QPushButton::clicked, this, &FolderDialog::chooseExcludes);
    auto *excludeRow = new QHBoxLayout;
    excludeRow->addWidget(m_excludes, 1);
    excludeRow->addWidget(chooseExcludes);
    auto *excludeHint =
        new QLabel(tr("Folders directly inside this one that cleaning never touches."), this);
    excludeHint->setWordWrap(true);

    auto *form = new QFormLayout;
    // macOS keeps fields at their size hint by default, which leaves the text fields narrow.
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("&Name:"), m_name);
    // A row given a layout gets no buddy, and the label would show its '&'.
    auto *pathLabel = new QLabel(tr("&Path:"), this);
    pathLabel->setBuddy(m_path);
    form->addRow(pathLabel, pathRow);
    form->addRow(tr("Cleans:"), kind);
    if (m_types)
        form->addRow(tr("&Types:"), m_types);
    auto *excludeLabel = new QLabel(tr("&Excluded:"), this);
    excludeLabel->setBuddy(chooseExcludes);
    form->addRow(excludeLabel, excludeRow);
    form->addRow(QString(), excludeHint);
    form->addRow(tr("&Warn above, to clean:"), m_warn);
    form->addRow(QString(), m_inWarnings);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_path, &QLineEdit::textChanged, this, &FolderDialog::updateOk);
    connect(m_name, &QLineEdit::textChanged, this, &FolderDialog::updateOk);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form, 1);
    layout->addWidget(buttons);
    updateOk();
}

Folder FolderDialog::folder() const
{
    Folder f = m_folder;
    f.name = m_name->text().trimmed();
    f.path = QDir::cleanPath(QDir::fromNativeSeparators(m_path->text().trimmed()));
    if (f.name.isEmpty())
        f.name = QFileInfo(f.path).fileName();
    if (m_types) {
        f.typeIds.clear();
        for (int i = 0; i < m_types->count(); ++i) {
            if (m_types->item(i)->checkState() == Qt::Checked)
                f.typeIds.append(m_types->item(i)->data(TypeIdRole).toString());
        }
    }
    f.warnBytes = m_warn->bytes();
    f.inWarnings = m_inWarnings->isChecked();
    return f;
}

void FolderDialog::chooseExcludes()
{
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(m_path->text().trimmed()));
    ExcludeDialog dialog(path, m_folder.excludes, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_folder.excludes = dialog.excludes();
    m_excludes->setText(ExcludeDialog::summary(m_folder.excludes));
}

void FolderDialog::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder"), m_path->text());
    if (dir.isEmpty())
        return;
    m_path->setText(QDir::toNativeSeparators(dir));
    if (m_name->text().trimmed().isEmpty())
        m_name->setText(QFileInfo(dir).fileName());
}

void FolderDialog::updateOk()
{
    // A Matching folder needs a type; without one it would delete nothing.
    bool ready = !m_types;
    for (int i = 0; m_types && i < m_types->count() && !ready; ++i)
        ready = m_types->item(i)->checkState() == Qt::Checked;
    m_ok->setEnabled(ready && !m_path->text().trimmed().isEmpty());
}
