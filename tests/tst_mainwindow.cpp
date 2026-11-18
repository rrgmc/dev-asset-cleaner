#include "clean/Monitor.h"
#include "model/Store.h"
#include "ui/DetectDialog.h"
#include "ui/FolderDelegate.h"
#include "ui/FolderDialog.h"
#include "ui/FolderListView.h"
#include "ui/FolderModel.h"
#include "ui/GroupListView.h"
#include "ui/GroupModel.h"
#include "ui/MainWindow.h"
#include "ui/SystemFoldersDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTest>

namespace {
void writeFile(const QString &path, int bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(bytes, 'x'));
}
} // namespace

// Drives the main window on the offscreen platform.
class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Keep QSettings (window geometry) away from the real application's.
        QCoreApplication::setOrganizationName(QStringLiteral("dev-asset-cleaner-tests"));
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        writeFile(m_dir->filePath(QStringLiteral("projects/app/Cargo.toml")), 10);
        writeFile(m_dir->filePath(QStringLiteral("projects/app/target/debug/app")), 4000);
        writeFile(m_dir->filePath(QStringLiteral("cache/blob")), 700);

        m_store = std::make_unique<Store>(m_dir->filePath(QStringLiteral("data.json")));
        m_store->setAutoSave(false);
        QVERIFY(m_store->load());
        m_monitor = std::make_unique<Monitor>(m_store.get());
        m_window = std::make_unique<MainWindow>(m_store.get(), m_monitor.get());
        m_window->setConfirmCleaning(false);
        m_window->showAndFocus();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
        m_list = m_window->findChild<FolderListView *>();
        QVERIFY(m_list);
        m_groups = m_window->findChild<GroupListView *>();
        QVERIFY(m_groups);
    }

    void cleanup()
    {
        m_window.reset();
        m_monitor.reset();
        m_store.reset();
        m_dir.reset();
    }

    void addedFoldersAreScanned()
    {
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching, {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole);
        QCOMPARE(m_store->folders().size(), 2);
        QCOMPARE(m_store->folders().at(1).kind, Folder::Kind::Whole);

        const QUuid projects = m_store->folders().at(0).id;
        QTRY_VERIFY(m_monitor->result(projects).valid);
        QCOMPARE(m_monitor->result(projects).cleanableBytes, 4000);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 4700);
        QCOMPARE(m_monitor->totalBytes(), 4710);

        // Adding the same folder again selects it instead.
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Whole);
        QCOMPARE(m_store->folders().size(), 2);
        QCOMPARE(m_list->currentId(), projects);
    }

    void cleanButtonCleansOneFolder()
    {
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching, {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 4700);

        const QModelIndex first = m_list->folderModel()->index(0);
        QTRY_VERIFY(first.data(FolderModel::CanCleanRole).toBool());
        const QRect button =
            FolderDelegate::buttonRect(m_list->visualRect(first), m_list->fontMetrics());
        QTest::mouseClick(m_list->viewport(), Qt::LeftButton, {}, button.center());

        const QUuid projects = m_store->folders().at(0).id;
        QTRY_VERIFY(!QFileInfo::exists(m_dir->filePath(QStringLiteral("projects/app/target"))));
        QTRY_COMPARE(m_monitor->state(projects), Monitor::State::Idle);
        QCOMPARE(m_monitor->result(projects).cleanableBytes, 0);
        QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("projects/app/Cargo.toml"))));
        QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("cache/blob"))));
    }

    void cleanAllCleansEveryFolder()
    {
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching, {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 4700);
        QTest::keyClick(m_window.get(), Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 0);
        QTRY_VERIFY(!m_monitor->isBusy());
        QVERIFY(QFileInfo(m_dir->filePath(QStringLiteral("cache"))).isDir());
        QVERIFY(!QFileInfo::exists(m_dir->filePath(QStringLiteral("cache/blob"))));
    }

    void groupsSplitTheList()
    {
        const QUuid first = m_store->groups().first().id;
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching, {QStringLiteral("rust")});
        const QUuid second = m_store->addGroup(QStringLiteral("Caches"));
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole, {}, {}, second);
        QCOMPARE(m_store->foldersIn({second}).size(), 1);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 4700);

        // Adding a folder selects its group, which shows only that group's folders.
        QCOMPARE(m_groups->selectedIds(), QList<QUuid>{second});
        QCOMPARE(m_list->model()->rowCount(), 1);

        // The usage panel adds the selected group's usage beside every folder's.
        auto *usage = m_window->findChild<QWidget *>(QStringLiteral("usagePanel"));
        QVERIFY(usage);
        QTRY_VERIFY(usage->toolTip().contains(QStringLiteral("Selected groups")));
        const int partialWidth = usage->sizeHint().width();

        // Both groups selected: both folders, in the groups' order.
        m_groups->selectAll();
        QCOMPARE(m_groups->selectedIds(), (QList<QUuid>{first, second}));
        QCOMPARE(m_list->model()->rowCount(), 2);
        // The selection is everything, so the panel shows only the total.
        QVERIFY(!usage->toolTip().contains(QStringLiteral("Selected groups")));
        QVERIFY(usage->sizeHint().width() < partialWidth);

        // Cleaning the selected groups leaves the other groups alone.
        m_groups->selectId(second);
        QTest::keyClick(m_window.get(), Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(!QFileInfo::exists(m_dir->filePath(QStringLiteral("cache/blob"))));
        QTRY_VERIFY(!m_monitor->isBusy());
        QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("projects/app/target"))));

        // A folder moved to another group follows it.
        const QUuid projects = m_store->foldersIn({first}).first().id;
        m_store->moveToGroup(projects, second);
        QCOMPARE(m_list->model()->rowCount(), 2);

        // Deleting a group stops monitoring its folders and selects a neighbour.
        m_store->removeGroup(second);
        QVERIFY(m_store->folders().isEmpty());
        QCOMPARE(m_groups->selectedIds(), QList<QUuid>{first});
        QCOMPARE(m_list->model()->rowCount(), 0);

        // With no group left there is nowhere to add folders.
        m_store->removeGroup(first);
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole);
        QVERIFY(m_store->folders().isEmpty());
    }

    void filterHidesOtherFolders()
    {
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching, {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole);
        auto *filter = m_window->findChild<QLineEdit *>();
        QVERIFY(filter);
        // The names, not the temporary path, which could contain anything.
        filter->setText(QStringLiteral("projects"));
        QCOMPARE(m_list->model()->rowCount(), 1);
        filter->clear();
        QCOMPARE(m_list->model()->rowCount(), 2);
    }

    void detectDialogSuggestsTypes()
    {
        writeFile(m_dir->filePath(QStringLiteral("projects/web/package.json")), 5);
        writeFile(m_dir->filePath(QStringLiteral("projects/web/node_modules/x.js")), 300);
        DetectDialog dialog(m_store.get(), m_dir->filePath(QStringLiteral("projects")));
        dialog.show();
        auto *tree = dialog.findChild<QTreeWidget *>();
        QVERIFY(tree);
        // Rust, JavaScript, and "All programming languages", which starts unchecked.
        QTRY_COMPARE(tree->topLevelItemCount(), 3);
        QCOMPARE(dialog.typeIds(), (QStringList{QStringLiteral("rust"), QStringLiteral("node")}));
        const QList<QTreeWidgetItem *> rust = tree->findItems(QStringLiteral("Rust"), Qt::MatchExactly);
        QCOMPARE(rust.size(), 1);
        rust.first()->setCheckState(0, Qt::Unchecked);
        QCOMPARE(dialog.typeIds(), QStringList{QStringLiteral("node")});
        QCOMPARE(dialog.path(), QDir::cleanPath(m_dir->filePath(QStringLiteral("projects"))));
        QVERIFY(dialog.excludes().isEmpty());
    }

    void detectDialogSearchesWithoutExcludedFolders()
    {
        writeFile(m_dir->filePath(QStringLiteral("projects/web/package.json")), 5);
        writeFile(m_dir->filePath(QStringLiteral("projects/web/node_modules/x.js")), 300);
        const QString projects = QDir::cleanPath(m_dir->filePath(QStringLiteral("projects")));
        // A folder in the list brings its excluded folders to the search.
        m_window->addFolderPath(projects, Folder::Kind::Matching, {QStringLiteral("rust")}, {}, {},
                                {QStringLiteral("app")});
        DetectDialog dialog(m_store.get(), projects);
        QCOMPARE(dialog.excludes(), QStringList{QStringLiteral("app")});
        dialog.show();
        auto *tree = dialog.findChild<QTreeWidget *>();
        QVERIFY(tree);
        // JavaScript, and "All programming languages", which starts unchecked.
        QTRY_COMPARE(tree->topLevelItemCount(), 2);
        QCOMPARE(dialog.typeIds(), QStringList{QStringLiteral("node")});
    }

    void folderDialogPicksTheKind()
    {
        Folder f;
        f.path = QDir::cleanPath(m_dir->filePath(QStringLiteral("projects")));
        f.kind = Folder::Kind::Matching;
        f.typeIds = {QStringLiteral("rust")};
        f.excludes = {QStringLiteral("app")};
        FolderDialog dialog(m_store.get(), f);
        auto *kind = dialog.findChild<QComboBox *>();
        auto *types = dialog.findChild<QListWidget *>();
        QVERIFY(kind && types);
        // Every entry in the list is a type that matches folders; the kind is chosen apart.
        QCOMPARE(types->count(), BuiltInTypes::all().size());
        QVERIFY(types->isEnabled());
        QCOMPARE(dialog.folder().typeIds, QStringList{QStringLiteral("rust")});

        kind->setCurrentIndex(kind->findData(static_cast<int>(Folder::Kind::Whole)));
        QVERIFY(!types->isEnabled());
        const Folder whole = dialog.folder();
        QCOMPARE(whole.kind, Folder::Kind::Whole);
        QVERIFY(whole.typeIds.isEmpty());
        // Both kinds keep their excluded folders.
        QCOMPARE(whole.excludes, QStringList{QStringLiteral("app")});
    }

    void systemFoldersDialogSkipsListedFolders()
    {
        const QString cache = QDir::cleanPath(m_dir->filePath(QStringLiteral("cache")));
        const QString trash = QDir::cleanPath(m_dir->filePath(QStringLiteral("trash")));
        QVERIFY(QDir().mkpath(trash));
        m_window->addFolderPath(cache, Folder::Kind::Whole);
        const QList<SystemFolder> folders{{QStringLiteral("Cache"), cache, QStringLiteral("c")},
                                          {QStringLiteral("Trash"), trash, QStringLiteral("t")}};
        SystemFoldersDialog dialog(m_store.get(), folders);
        auto *tree = dialog.findChild<QTreeWidget *>();
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 2);
        // The folder already in the list cannot be picked.
        QVERIFY(!(tree->topLevelItem(0)->flags() & Qt::ItemIsEnabled));
        QCOMPARE(dialog.chosen().size(), 1);
        QCOMPARE(dialog.chosen().first().path, trash);
        tree->topLevelItem(1)->setCheckState(0, Qt::Unchecked);
        QVERIFY(dialog.chosen().isEmpty());
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Store> m_store;
    std::unique_ptr<Monitor> m_monitor;
    std::unique_ptr<MainWindow> m_window;
    FolderListView *m_list = nullptr;
    GroupListView *m_groups = nullptr;
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
