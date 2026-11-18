#include "FolderDialog.h"

#include "ItemCheckStyle.h"
#include "SizeSpinBox.h"
#include "model/Store.h"

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
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
constexpr int TypeIdRole = Qt::UserRole + 1;
}

FolderDialog::FolderDialog(Store *store, const Folder &folder, QWidget *parent)
    : QDialog(parent)
    , m_folder(folder)
    , m_name(new QLineEdit(folder.name, this))
    , m_path(new QLineEdit(QDir::toNativeSeparators(folder.path), this))
    , m_types(new QListWidget(this))
    , m_warn(new SizeSpinBox(this))
{
    setWindowTitle(tr("Folder"));
    resize(520, 520);

    auto *browse = new QPushButton(tr("&Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &FolderDialog::browse);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);

    const QStringList selected =
        folder.typeIds.isEmpty() ? QStringList{BuiltInTypes::contentsId()} : folder.typeIds;
    for (const CleanerType &t : store->types()) {
        auto *item = new QListWidgetItem(t.name, m_types);
        item->setData(TypeIdRole, t.id);
        item->setToolTip(t.description);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(selected.contains(t.id) ? Qt::Checked : Qt::Unchecked);
    }
    ItemCheckStyle::apply(m_types);
    connect(m_types, &QListWidget::itemChanged, this,
            [this](QListWidgetItem *item) { typeChanged(m_types->row(item)); });

    m_warn->setBytes(folder.warnBytes);

    auto *typesHint = new QLabel(
        tr("\"Folder contents\" deletes everything inside the folder. The other types delete only "
           "the matching build and dependency folders, at any depth."),
        this);
    typesHint->setWordWrap(true);

    auto *form = new QFormLayout;
    // macOS keeps fields at their size hint by default, which leaves the text fields narrow.
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("&Name:"), m_name);
    // A row given a layout gets no buddy, and the label would show its '&'.
    auto *pathLabel = new QLabel(tr("&Path:"), this);
    pathLabel->setBuddy(m_path);
    form->addRow(pathLabel, pathRow);
    form->addRow(tr("&Types:"), m_types);
    form->addRow(QString(), typesHint);
    form->addRow(tr("&Warn above:"), m_warn);

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
    f.typeIds.clear();
    for (int i = 0; i < m_types->count(); ++i) {
        if (m_types->item(i)->checkState() == Qt::Checked)
            f.typeIds.append(m_types->item(i)->data(TypeIdRole).toString());
    }
    f.warnBytes = m_warn->bytes();
    return f;
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

void FolderDialog::typeChanged(int row)
{
    // "Folder contents" already covers everything, so it excludes the other types.
    QListWidgetItem *changed = m_types->item(row);
    if (!changed || changed->checkState() != Qt::Checked)
        return updateOk();
    const bool isContents = changed->data(TypeIdRole).toString() == BuiltInTypes::contentsId();
    const QSignalBlocker blocker(m_types);
    for (int i = 0; i < m_types->count(); ++i) {
        QListWidgetItem *item = m_types->item(i);
        if (item == changed)
            continue;
        const bool otherIsContents = item->data(TypeIdRole).toString() == BuiltInTypes::contentsId();
        if (isContents || otherIsContents)
            item->setCheckState(Qt::Unchecked);
    }
    m_types->viewport()->update();
    updateOk();
}

void FolderDialog::updateOk()
{
    bool anyType = false;
    for (int i = 0; i < m_types->count(); ++i)
        anyType = anyType || m_types->item(i)->checkState() == Qt::Checked;
    m_ok->setEnabled(anyType && !m_path->text().trimmed().isEmpty());
}
