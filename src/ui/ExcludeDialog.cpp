#include "ExcludeDialog.h"

#include "ItemCheckStyle.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

ExcludeDialog::ExcludeDialog(const QString &path, const QStringList &excludes, QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
{
    setWindowTitle(tr("Excluded folders"));
    resize(420, 420);

    auto *hint = new QLabel(tr("Check the folders inside %1 that cleaning must never touch. "
                               "They are not searched, and nothing in them is deleted.")
                                .arg(QDir::toNativeSeparators(path)),
                            this);
    hint->setWordWrap(true);

    QStringList names = QDir(path).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QString &name : excludes) {
        if (!names.contains(name, Qt::CaseInsensitive))
            names.append(name);
    }
    names.sort(Qt::CaseInsensitive);
    for (const QString &name : names) {
        auto *item = new QListWidgetItem(name, m_list);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(excludes.contains(name, Qt::CaseInsensitive) ? Qt::Checked
                                                                         : Qt::Unchecked);
        if (!QFileInfo(QDir(path).filePath(name)).isDir())
            item->setToolTip(tr("The folder does not exist now."));
    }
    ItemCheckStyle::apply(m_list);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_list, 1);
    layout->addWidget(buttons);
}

QStringList ExcludeDialog::excludes() const
{
    QStringList names;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->checkState() == Qt::Checked)
            names.append(m_list->item(i)->text());
    }
    return names;
}

QString ExcludeDialog::summary(const QStringList &excludes)
{
    if (excludes.isEmpty())
        return tr("None");
    return excludes.join(QStringLiteral(", "));
}
