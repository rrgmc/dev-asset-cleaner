#include "TypesDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace {
constexpr int TypeIdRole = Qt::UserRole + 1;

QStringList splitMarkers(const QString &text)
{
    QStringList markers;
    for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString m = part.trimmed();
        if (!m.isEmpty())
            markers.append(m);
    }
    return markers;
}
} // namespace

TypesDialog::TypesDialog(const QList<CleanerType> &customTypes, QWidget *parent)
    : QDialog(parent)
    , m_custom(customTypes)
    , m_list(new QListWidget(this))
    , m_duplicate(new QPushButton(tr("&Duplicate"), this))
    , m_remove(new QPushButton(tr("&Delete"), this))
    , m_name(new QLineEdit(this))
    , m_description(new QLineEdit(this))
    , m_rules(new QTableWidget(0, 2, this))
    , m_addRule(new QPushButton(tr("Add &rule"), this))
    , m_removeRule(new QPushButton(tr("Remove r&ule"), this))
    , m_readOnly(new QLabel(this))
{
    setWindowTitle(tr("Cleaner types"));
    resize(860, 520);

    auto *add = new QPushButton(tr("&New"), this);
    connect(add, &QPushButton::clicked, this, [this] {
        CleanerType t;
        t.name = tr("New type");
        t.rules.append(CleanRule{QStringLiteral("build"), {QStringLiteral("Makefile")}});
        addType(t);
    });
    connect(m_duplicate, &QPushButton::clicked, this, [this] {
        storeEditor();
        const int row = m_list->currentRow();
        if (row < 0)
            return;
        const QString id = m_list->item(row)->data(TypeIdRole).toString();
        for (const CleanerType &t : BuiltInTypes::all() + m_custom) {
            if (t.id == id) {
                CleanerType copy = t;
                copy.name = tr("%1 (copy)").arg(t.name);
                addType(copy);
                return;
            }
        }
    });
    connect(m_remove, &QPushButton::clicked, this, &TypesDialog::removeType);

    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(add);
    listButtons->addWidget(m_duplicate);
    listButtons->addWidget(m_remove);
    auto *left = new QVBoxLayout;
    left->addWidget(m_list, 1);
    left->addLayout(listButtons);

    m_rules->setHorizontalHeaderLabels({tr("Folder name"), tr("Only next to (comma-separated)")});
    m_rules->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_rules->horizontalHeader()->setStretchLastSection(true);
    m_rules->setColumnWidth(0, 170);
    m_rules->verticalHeader()->hide();
    m_rules->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rules->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_addRule, &QPushButton::clicked, this, &TypesDialog::addRule);
    connect(m_removeRule, &QPushButton::clicked, this, &TypesDialog::removeRule);
    auto *ruleButtons = new QHBoxLayout;
    ruleButtons->addWidget(m_addRule);
    ruleButtons->addWidget(m_removeRule);
    ruleButtons->addStretch(1);

    auto *rulesHint = new QLabel(
        tr("A rule deletes folders with this name that sit next to a file matching one of the "
           "names in the second column, such as target next to Cargo.toml. Leave the second "
           "column empty to match the name anywhere. Both take wildcards: * and ?."),
        this);
    rulesHint->setWordWrap(true);
    m_readOnly->setWordWrap(true);

    auto *form = new QFormLayout;
    // macOS keeps fields at their size hint by default, which leaves the text fields narrow.
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("&Name:"), m_name);
    form->addRow(tr("D&escription:"), m_description);
    auto *right = new QVBoxLayout;
    right->addWidget(m_readOnly);
    right->addLayout(form);
    right->addWidget(m_rules, 1);
    right->addLayout(ruleButtons);
    right->addWidget(rulesHint);

    auto *columns = new QHBoxLayout;
    columns->addLayout(left, 1);
    columns->addLayout(right, 2);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        storeEditor();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(columns, 1);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, &TypesDialog::currentRowChanged);
    connect(m_name, &QLineEdit::editingFinished, this, [this] {
        storeEditor();
        if (QListWidgetItem *item = m_list->currentItem(); item && currentCustom())
            item->setText(currentCustom()->name);
    });

    rebuildList(BuiltInTypes::all().first().id);
}

QList<CleanerType> TypesDialog::customTypes() const
{
    return m_custom;
}

void TypesDialog::rebuildList(const QString &selectId)
{
    m_loading = true;
    m_list->clear();
    for (const CleanerType &t : BuiltInTypes::all() + m_custom) {
        auto *item = new QListWidgetItem(t.builtIn ? tr("%1 (built-in)").arg(t.name) : t.name,
                                         m_list);
        item->setData(TypeIdRole, t.id);
        item->setToolTip(t.description);
    }
    m_loading = false;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(TypeIdRole).toString() == selectId) {
            m_list->setCurrentRow(i);
            currentRowChanged(i);
            return;
        }
    }
    m_list->setCurrentRow(0);
}

void TypesDialog::currentRowChanged(int row)
{
    if (m_loading)
        return;
    storeEditor();
    m_currentId = row >= 0 ? m_list->item(row)->data(TypeIdRole).toString() : QString();
    for (const CleanerType &t : BuiltInTypes::all() + m_custom) {
        if (t.id == m_currentId) {
            loadEditor(t);
            return;
        }
    }
}

void TypesDialog::loadEditor(const CleanerType &type)
{
    m_loading = true;
    m_name->setText(type.name);
    m_description->setText(type.description);
    m_rules->setRowCount(0);
    for (const CleanRule &r : type.rules) {
        const int row = m_rules->rowCount();
        m_rules->insertRow(row);
        m_rules->setItem(row, 0, new QTableWidgetItem(r.pattern));
        m_rules->setItem(row, 1, new QTableWidgetItem(r.markers.join(QStringLiteral(", "))));
    }
    m_readOnly->setText(type.builtIn
                            ? tr("Built-in types cannot be changed. Duplicate one to make your own.")
                            : QString());
    m_readOnly->setVisible(type.builtIn);
    m_loading = false;
    updateEnabled();
}

CleanerType *TypesDialog::currentCustom()
{
    for (CleanerType &t : m_custom) {
        if (t.id == m_currentId)
            return &t;
    }
    return nullptr;
}

void TypesDialog::storeEditor()
{
    CleanerType *t = currentCustom();
    if (!t || m_loading)
        return;
    t->name = m_name->text().trimmed().isEmpty() ? tr("Unnamed type") : m_name->text().trimmed();
    t->description = m_description->text().trimmed();
    t->rules.clear();
    for (int row = 0; row < m_rules->rowCount(); ++row) {
        const QTableWidgetItem *pattern = m_rules->item(row, 0);
        const QTableWidgetItem *markers = m_rules->item(row, 1);
        const QString p = pattern ? pattern->text().trimmed() : QString();
        if (!p.isEmpty())
            t->rules.append(CleanRule{p, splitMarkers(markers ? markers->text() : QString())});
    }
}

void TypesDialog::addType(CleanerType type)
{
    storeEditor();
    type.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    type.builtIn = false;
    m_custom.append(type);
    m_currentId.clear();
    rebuildList(type.id);
    m_name->setFocus();
    m_name->selectAll();
}

void TypesDialog::removeType()
{
    const CleanerType *t = currentCustom();
    if (!t)
        return;
    const QString id = t->id;
    m_custom.removeIf([&id](const CleanerType &c) { return c.id == id; });
    m_currentId.clear();
    rebuildList(BuiltInTypes::all().first().id);
}

void TypesDialog::addRule()
{
    const int row = m_rules->rowCount();
    m_rules->insertRow(row);
    m_rules->setItem(row, 0, new QTableWidgetItem);
    m_rules->setItem(row, 1, new QTableWidgetItem);
    m_rules->setCurrentCell(row, 0);
    m_rules->editItem(m_rules->item(row, 0));
}

void TypesDialog::removeRule()
{
    const int row = m_rules->currentRow();
    if (row >= 0)
        m_rules->removeRow(row);
}

void TypesDialog::updateEnabled()
{
    const bool editable = currentCustom() != nullptr;
    m_name->setReadOnly(!editable);
    m_description->setReadOnly(!editable);
    m_rules->setEditTriggers(editable ? QAbstractItemView::AllEditTriggers
                                      : QAbstractItemView::NoEditTriggers);
    m_addRule->setEnabled(editable);
    m_removeRule->setEnabled(editable);
    m_remove->setEnabled(editable);
    m_duplicate->setEnabled(!m_currentId.isEmpty());
}
