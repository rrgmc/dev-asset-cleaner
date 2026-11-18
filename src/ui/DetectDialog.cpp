#include "DetectDialog.h"

#include "ExcludeDialog.h"
#include "Format.h"
#include "ItemCheckStyle.h"
#include "model/Store.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int TypeIdRole = Qt::UserRole + 1;
constexpr int MaxItemsPerType = 50;  // the folders listed under each type
} // namespace

DetectDialog::DetectDialog(Store *store, const QString &path, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_path(new QLineEdit(QDir::toNativeSeparators(path), this))
    , m_search(new QPushButton(tr("&Search"), this))
    , m_exclude(new QPushButton(tr("E&xclude…"), this))
    , m_excludeLabel(new QLabel(this))
    , m_status(new QLabel(this))
    , m_results(new QTreeWidget(this))
    , m_autoStart(!path.isEmpty())
{
    setWindowTitle(tr("Detect cleaner types"));
    resize(680, 520);

    auto *browse = new QPushButton(tr("&Browse…"), this);
    connect(browse, &QPushButton::clicked, this, &DetectDialog::browse);
    connect(m_search, &QPushButton::clicked, this, [this] {
        if (m_watcher.isRunning())
            cancel();
        else
            start();
    });
    connect(m_path, &QLineEdit::returnPressed, this, &DetectDialog::start);
    connect(m_path, &QLineEdit::textChanged, this, &DetectDialog::updateButtons);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(new QLabel(tr("&Folder:"), this));
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);
    pathRow->addWidget(m_search);
    static_cast<QLabel *>(pathRow->itemAt(0)->widget())->setBuddy(m_path);

    auto *hint = new QLabel(tr("Searches the folder for build outputs and dependency folders of "
                               "programming languages. Check the types the folder should use."),
                            this);
    hint->setWordWrap(true);
    m_status->setWordWrap(true);

    m_excludeLabel->setWordWrap(true);
    m_excludeLabel->setTextFormat(Qt::PlainText);
    connect(m_exclude, &QPushButton::clicked, this, &DetectDialog::chooseExcludes);
    auto *excludeRow = new QHBoxLayout;
    excludeRow->addWidget(m_excludeLabel, 1);
    excludeRow->addWidget(m_exclude);
    if (const Folder *f = store->folderWithPath(this->path())) {
        m_excludes = f->excludes;
        m_excludesPath = this->path();
    }
    m_excludeLabel->setText(tr("Excluded folders: %1").arg(ExcludeDialog::summary(m_excludes)));

    m_results->setHeaderLabels({tr("Type"), tr("Folders"), tr("Size")});
    m_results->header()->setStretchLastSection(false);
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_results->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_results->setUniformRowHeights(true);
    ItemCheckStyle::apply(m_results);
    connect(m_results, &QTreeWidget::itemChanged, this, &DetectDialog::updateButtons);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(store->folderWithPath(path) ? tr("&Use these types") : tr("&Add folder"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(&m_watcher, &QFutureWatcher<DetectResult>::finished, this, &DetectDialog::finished);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(pathRow);
    layout->addWidget(hint);
    layout->addLayout(excludeRow);
    layout->addWidget(m_results, 1);
    layout->addWidget(m_status);
    layout->addWidget(buttons);
    updateButtons();
}

DetectDialog::~DetectDialog()
{
    if (m_cancel)
        m_cancel->store(true);
    m_watcher.waitForFinished();
}

QString DetectDialog::path() const
{
    return QDir::cleanPath(QDir::fromNativeSeparators(m_path->text().trimmed()));
}

QStringList DetectDialog::typeIds() const
{
    QStringList checked;
    for (int i = 0; i < m_results->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *item = m_results->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
            checked.append(item->data(0, TypeIdRole).toString());
    }
    QStringList ordered;
    for (const CleanerType &t : m_store->types()) {
        if (checked.contains(t.id))
            ordered.append(t.id);
    }
    return ordered;
}

void DetectDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (m_autoStart) {
        m_autoStart = false;
        QTimer::singleShot(0, this, &DetectDialog::start);
    }
}

void DetectDialog::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder"), m_path->text());
    if (dir.isEmpty())
        return;
    m_path->setText(QDir::toNativeSeparators(dir));
    start();
}

QStringList DetectDialog::excludes() const
{
    return m_excludesPath == path() ? m_excludes : QStringList();
}

void DetectDialog::setExcludes(const QStringList &excludes)
{
    m_excludes = excludes;
    m_excludesPath = path();
    m_excludeLabel->setText(tr("Excluded folders: %1").arg(ExcludeDialog::summary(m_excludes)));
}

void DetectDialog::chooseExcludes()
{
    if (m_watcher.isRunning() || path().isEmpty())
        return;
    ExcludeDialog dialog(path(), m_excludesPath == path() ? m_excludes : QStringList(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    setExcludes(dialog.excludes());
    start();
}

void DetectDialog::start()
{
    if (m_watcher.isRunning() || path().isEmpty())
        return;
    const QString folder = path();
    // Exclusions belong to one folder: another one starts with its own, or none.
    if (m_excludesPath != folder) {
        const Folder *f = m_store->folderWithPath(folder);
        setExcludes(f ? f->excludes : QStringList());
    }
    m_results->clear();
    m_status->setText(tr("Searching…"));
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancelFlag = m_cancel;
    const QList<CleanerType> types = m_store->types();
    const QStringList excludes = m_excludes;
    m_watcher.setFuture(QtConcurrent::run([folder, types, excludes, cancelFlag] {
        return Scanner::detect(folder, types, [cancelFlag] { return cancelFlag->load(); },
                               excludes);
    }));
    m_ok->setText(m_store->folderWithPath(folder) ? tr("&Use these types") : tr("&Add folder"));
    updateButtons();
}

void DetectDialog::cancel()
{
    if (m_cancel)
        m_cancel->store(true);
}

void DetectDialog::finished()
{
    const DetectResult result = m_watcher.result();
    m_results->clear();
    if (!result.valid) {
        m_status->setText(tr("Search canceled."));
    } else if (!result.error.isEmpty()) {
        m_status->setText(result.error);
    } else if (result.detections.isEmpty()) {
        m_status->setText(tr("The folder holds %1, but no build outputs or dependency folders "
                             "were found in it.")
                              .arg(Format::bytes(result.totalBytes)));
    } else {
        m_status->setText(tr("The folder holds %1. Cleaning with every type found would free %2.")
                              .arg(Format::bytes(result.totalBytes),
                                   Format::bytes(result.cleanableBytes)));
    }

    const QDir base(path());
    const QSignalBlocker blocker(m_results);
    for (const Detection &d : result.detections) {
        const CleanerType *type = m_store->type(d.typeId);
        if (!type)
            continue;
        auto *row = new QTreeWidgetItem(m_results);
        row->setText(0, type->name);
        row->setToolTip(0, type->description);
        row->setData(0, TypeIdRole, d.typeId);
        row->setText(1, QString::number(d.items.size()));
        row->setText(2, Format::bytes(d.bytes));
        row->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        row->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        // "All programming languages" repeats the others; leave it for the user to choose.
        row->setCheckState(0, d.typeId == QLatin1String("all-languages") ? Qt::Unchecked
                                                                          : Qt::Checked);
        for (qsizetype i = 0; i < d.items.size() && i < MaxItemsPerType; ++i) {
            auto *child = new QTreeWidgetItem(row);
            child->setText(0, QDir::toNativeSeparators(base.relativeFilePath(d.items.at(i).path)));
            child->setToolTip(0, QDir::toNativeSeparators(d.items.at(i).path));
            child->setText(2, Format::bytes(d.items.at(i).bytes));
            child->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }
        if (d.items.size() > MaxItemsPerType) {
            auto *more = new QTreeWidgetItem(row);
            more->setText(0, tr("…and %n more", nullptr,
                                static_cast<int>(d.items.size() - MaxItemsPerType)));
        }
    }
    updateButtons();
}

void DetectDialog::updateButtons()
{
    const bool running = m_watcher.isRunning();
    m_search->setText(running ? tr("S&top") : tr("&Search"));
    m_search->setEnabled(running || !m_path->text().trimmed().isEmpty());
    m_path->setEnabled(!running);
    m_exclude->setEnabled(!running && !m_path->text().trimmed().isEmpty());
    m_ok->setEnabled(!running && !typeIds().isEmpty());
}
