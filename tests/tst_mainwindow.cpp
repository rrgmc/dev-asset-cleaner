#include "clean/Monitor.h"
#include "model/Store.h"
#include "ui/CleanableListView.h"
#include "ui/DetectDialog.h"
#include "ui/FolderDelegate.h"
#include "ui/FolderDialog.h"
#include "ui/FolderListView.h"
#include "ui/FolderModel.h"
#include "ui/FreeSpaceDialog.h"
#include "ui/GroupListView.h"
#include "ui/GroupModel.h"
#include "ui/MainWindow.h"
#include "ui/SubfolderDialog.h"
#include "ui/SubfolderModel.h"
#include "ui/SystemFoldersDialog.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolBar>
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
        QSettings().remove(QStringLiteral("view"));
    }

    void cleanupTestCase()
    {
        QSettings().remove(QStringLiteral("view"));
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
        if (m_window) {
            m_window->setGroupSort(SortOrder::Manual);
            m_window->setFolderSort(SortOrder::Manual);
        }
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

        // A folder's warning measures its space to clean (4000 bytes), not its size (4010).
        Folder f = *m_store->folder(projects);
        f.warnBytes = 4005;
        m_store->updateFolder(f);
        QTRY_VERIFY(m_monitor->result(projects).valid);
        QVERIFY(!m_monitor->warnings().largeFolders.contains(projects));
        f.warnBytes = 3999;
        m_store->updateFolder(f);
        QTRY_VERIFY(m_monitor->result(projects).valid);
        QVERIFY(m_monitor->warnings().largeFolders.contains(projects));
        f.warnBytes = 0;
        m_store->updateFolder(f);

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

    void subfolderDialogCleansOneSubfolder()
    {
        writeFile(m_dir->filePath(QStringLiteral("projects/lib/Cargo.toml")), 10);
        writeFile(m_dir->filePath(QStringLiteral("projects/lib/target/out")), 300);
        writeFile(m_dir->filePath(QStringLiteral("projects/keep/Cargo.toml")), 10);
        writeFile(m_dir->filePath(QStringLiteral("projects/keep/target/out")), 50);
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching,
                                {QStringLiteral("rust")});
        Folder f = m_store->folders().at(0);
        QTRY_VERIFY(m_monitor->result(f.id).valid);
        QTRY_COMPARE(m_monitor->state(f.id), Monitor::State::Idle);
        f.excludes = {QStringLiteral("keep")};
        m_store->updateFolder(f);
        m_monitor->rescan(f.id);
        QTRY_COMPARE(m_monitor->result(f.id).cleanableBytes, 4300);
        QTRY_COMPARE(m_monitor->state(f.id), Monitor::State::Idle);

        SubfolderDialog dialog(m_store.get(), m_monitor.get(), f.id, m_window.get());
        dialog.setConfirmCleaning(false);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        SubfolderModel *model = dialog.model();
        QCOMPARE(model->rowCount(), 3);
        QTRY_VERIFY(!model->isBusy());
        const int app = model->rowOf(QStringLiteral("app"));
        const int keep = model->rowOf(QStringLiteral("keep"));
        const int lib = model->rowOf(QStringLiteral("lib"));
        QCOMPARE(model->resultAt(app).cleanableBytes, 4000);
        QCOMPARE(model->resultAt(lib).cleanableBytes, 300);
        // An excluded subfolder is sized but has nothing to clean.
        QVERIFY(model->isExcluded(keep));
        QCOMPARE(model->resultAt(keep).totalBytes, 60);
        QCOMPARE(model->resultAt(keep).cleanableBytes, 0);
        QVERIFY(!model->index(keep).data(FolderModel::CanCleanRole).toBool());

        // Click lib's Clean button. The list is sorted by name: app, keep, lib.
        auto *list = dialog.findChild<CleanableListView *>();
        QVERIFY(list);
        const QModelIndex row = list->model()->index(2, 0);
        QCOMPARE(row.data().toString(), QStringLiteral("lib"));
        const QRect button = FolderDelegate::buttonRect(list->visualRect(row), list->fontMetrics());
        QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, button.center());

        QTRY_VERIFY(!QFileInfo::exists(m_dir->filePath(QStringLiteral("projects/lib/target"))));
        QTRY_VERIFY(!model->isBusy());
        QCOMPARE(model->resultAt(lib).cleanableBytes, 0);
        QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("projects/app/target/debug/app"))));
        QVERIFY(QFileInfo::exists(m_dir->filePath(QStringLiteral("projects/keep/target/out"))));
        // The folder's own scan follows.
        QTRY_COMPARE(m_monitor->result(f.id).cleanableBytes, 4000);

        // Excluded subfolders are never cleaned, even when asked directly.
        model->clean(keep);
        QVERIFY(!model->isBusy());
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

    void groupsSort()
    {
        const QUuid first = m_store->groups().first().id;  // "Default"
        const QUuid ten = m_store->addGroup(QStringLiteral("group 10"));
        const QUuid nine = m_store->addGroup(QStringLiteral("Group 9"));
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole, {}, {}, nine);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 700);
        m_groups->selectId(ten);

        const auto rows = [this] {
            QList<QUuid> ids;
            for (int row = 0; row < m_groups->model()->rowCount(); ++row)
                ids.append(m_groups->model()->index(row, 0).data(GroupModel::IdRole).toUuid());
            return ids;
        };
        QAction *moveUp = findAction(QStringLiteral("Move &up"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up));
        QVERIFY(moveUp);
        QCOMPARE(rows(), (QList<QUuid>{first, ten, nine}));
        QVERIFY(moveUp->isEnabled());

        // By name, ignoring case, with numbers in numeric order. The selection stays.
        m_window->setGroupSort(SortOrder::Name);
        QCOMPARE(rows(), (QList<QUuid>{first, nine, ten}));
        QCOMPARE(m_groups->selectedIds(), QList<QUuid>{ten});
        QVERIFY(!moveUp->isEnabled());

        // A rename moves the group.
        m_store->renameGroup(first, QStringLiteral("Zeta"));
        QCOMPARE(rows(), (QList<QUuid>{nine, ten, first}));
        QCOMPARE(m_groups->selectedIds(), QList<QUuid>{ten});

        // By space to clean, the most first; ties keep the manual order.
        m_window->setGroupSort(SortOrder::Cleanable);
        QCOMPARE(rows(), (QList<QUuid>{nine, first, ten}));

        m_window->setGroupSort(SortOrder::Manual);
        QCOMPARE(rows(), (QList<QUuid>{first, ten, nine}));
        QVERIFY(moveUp->isEnabled());
    }

    void foldersSort()
    {
        writeFile(m_dir->filePath(QStringLiteral("other/blob")), 9000);
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole, {},
                                QStringLiteral("item 10"));
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching,
                                {QStringLiteral("rust")}, QStringLiteral("Item 9"));
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("other")), Folder::Kind::Whole, {},
                                QStringLiteral("Item 11"));
        QTRY_COMPARE(m_monitor->cleanableBytes(), 13700);
        QTRY_VERIFY(!m_monitor->isBusy());

        const auto names = [this] {
            QStringList list;
            for (int row = 0; row < m_list->model()->rowCount(); ++row)
                list.append(m_list->model()->index(row, 0).data().toString());
            return list;
        };
        QCOMPARE(names(), (QStringList{"item 10", "Item 9", "Item 11"}));
        const QUuid selected = m_store->folders().at(1).id;
        m_list->selectId(selected);

        m_window->setFolderSort(SortOrder::Name);
        QCOMPARE(names(), (QStringList{"Item 9", "item 10", "Item 11"}));
        QCOMPARE(m_list->currentId(), selected);
        QAction *moveUp = findAction(QStringLiteral("Move &up"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up));
        QVERIFY(moveUp);
        QVERIFY(!moveUp->isEnabled());

        m_window->setFolderSort(SortOrder::Cleanable);
        QCOMPARE(names(), (QStringList{"Item 11", "Item 9", "item 10"}));
        QCOMPARE(m_list->currentId(), selected);

        // A clean changes the space to clean, and the folder moves.
        m_monitor->clean(m_store->folders().at(2).id);
        QTRY_COMPARE(names(), (QStringList{"Item 9", "item 10", "Item 11"}));
        QTRY_VERIFY(!m_monitor->isBusy());
        QCOMPARE(m_list->currentId(), selected);

        // The choice is remembered by the next window.
        m_window.reset();
        m_window = std::make_unique<MainWindow>(m_store.get(), m_monitor.get());
        m_list = m_window->findChild<FolderListView *>();
        m_groups = m_window->findChild<GroupListView *>();
        QCOMPARE(names(), (QStringList{"Item 9", "item 10", "Item 11"}));
        m_window->setFolderSort(SortOrder::Manual);
        QCOMPARE(names(), (QStringList{"item 10", "Item 9", "Item 11"}));
        QVERIFY(moveUp = findAction(QStringLiteral("Move &up"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up)));
        m_list->selectId(selected);
        QVERIFY(moveUp->isEnabled());
    }

    void manualMoves()
    {
        const QUuid first = m_store->groups().first().id;
        const QUuid second = m_store->addGroup(QStringLiteral("Second"));
        m_groups->selectId(second);
        QAction *groupUp = findAction(QStringLiteral("Move &up"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up));
        QVERIFY(groupUp);
        QVERIFY(groupUp->isEnabled());
        groupUp->trigger();
        QCOMPARE(m_store->groups().first().id, second);
        QCOMPARE(m_groups->model()->index(0, 0).data(GroupModel::IdRole).toUuid(), second);
        QCOMPARE(m_groups->selectedIds(), QList<QUuid>{second});
        QVERIFY(groupUp->isEnabled());

        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")), Folder::Kind::Whole, {}, QStringLiteral("A"));
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), Folder::Kind::Matching,
                                {QStringLiteral("rust")}, QStringLiteral("B"));
        const auto names = [this] {
            QStringList list;
            for (int row = 0; row < m_list->model()->rowCount(); ++row)
                list.append(m_list->model()->index(row, 0).data().toString());
            return list;
        };
        QCOMPARE(names(), (QStringList{"A", "B"}));
        const QUuid b = m_store->folders().at(1).id;
        m_list->selectId(b);
        QAction *folderUp = findAction(QStringLiteral("Move &up"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up));
        QVERIFY(folderUp);
        QVERIFY(folderUp->isEnabled());
        folderUp->trigger();
        QCOMPARE(names(), (QStringList{"B", "A"}));
        QCOMPARE(m_list->currentId(), b);
        QVERIFY(folderUp->isEnabled());

        // The shortcuts work while the lists have the focus.
        m_list->setFocus();
        QTest::keyClick(m_list, Qt::Key_Down, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(names(), (QStringList{"A", "B"}));
        m_groups->setFocus();
        QTest::keyClick(m_groups, Qt::Key_Down, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(m_store->groups().last().id, second);

        // A drop within a list moves the row there and keeps it current.
        emit m_list->folderMoveRequested(b, m_store->folders().at(0).id, false);
        QCOMPARE(names(), (QStringList{"B", "A"}));
        QCOMPARE(m_list->currentId(), b);
        emit m_groups->groupMoveRequested(second, first, false);
        QCOMPARE(m_store->groups().first().id, second);
        QCOMPARE(m_groups->selectedIds(), QList<QUuid>{second});

        // The manual order comes back after another sort order.
        m_window->setFolderSort(SortOrder::Name);
        QCOMPARE(names(), (QStringList{"A", "B"}));
        m_window->setFolderSort(SortOrder::Manual);
        QCOMPARE(names(), (QStringList{"B", "A"}));
    }

    void sortButtons()
    {
        auto *toolbar = m_window->findChild<QToolBar *>();
        QVERIFY(toolbar);
        QAction *groups = nullptr;
        QAction *folders = nullptr;
        for (QAction *a : toolbar->actions()) {
            if (a->text() == QStringLiteral("Sort groups"))
                groups = a;
            else if (a->text() == QStringLiteral("Sort folders"))
                folders = a;
        }
        QVERIFY(groups && groups->menu());
        QVERIFY(folders && folders->menu());
        QCOMPARE(groups->iconText(), QStringLiteral("Sort groups: Manual order"));

        // Choosing from the button's menu sorts the list and changes the tooltip.
        for (QAction *a : folders->menu()->actions()) {
            if (a->data().toString() == QStringLiteral("name"))
                a->trigger();
        }
        QCOMPARE(m_list->folderModel()->sortOrder(), SortOrder::Name);
        QCOMPARE(folders->iconText(), QStringLiteral("Sort folders: Name"));
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

    void folderDialogKeepsTheKind()
    {
        Folder f;
        f.path = QDir::cleanPath(m_dir->filePath(QStringLiteral("projects")));
        f.kind = Folder::Kind::Matching;
        f.typeIds = {QStringLiteral("rust")};
        f.excludes = {QStringLiteral("app")};
        {
            FolderDialog dialog(m_store.get(), f);
            // Nothing offers another kind.
            QVERIFY(!dialog.findChild<QComboBox *>());
            auto *types = dialog.findChild<QListWidget *>();
            QVERIFY(types);
            QCOMPARE(types->count(), BuiltInTypes::all().size());
            const Folder edited = dialog.folder();
            QCOMPARE(edited.kind, Folder::Kind::Matching);
            QCOMPARE(edited.typeIds, QStringList{QStringLiteral("rust")});
            QCOMPARE(edited.excludes, QStringList{QStringLiteral("app")});
        }

        // A whole folder has no types to pick, and keeps its excluded folders too.
        f.kind = Folder::Kind::Whole;
        f.typeIds.clear();
        FolderDialog dialog(m_store.get(), f);
        QVERIFY(!dialog.findChild<QComboBox *>());
        QVERIFY(!dialog.findChild<QListWidget *>());
        const Folder whole = dialog.folder();
        QCOMPARE(whole.kind, Folder::Kind::Whole);
        QVERIFY(whole.typeIds.isEmpty());
        QCOMPARE(whole.excludes, QStringList{QStringLiteral("app")});
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons && buttons->button(QDialogButtonBox::Ok)->isEnabled());
    }

    void detectDialogLeavesWholeFoldersAlone()
    {
        writeFile(m_dir->filePath(QStringLiteral("projects/web/package.json")), 5);
        const QString projects = QDir::cleanPath(m_dir->filePath(QStringLiteral("projects")));
        m_window->addFolderPath(projects, Folder::Kind::Whole);
        DetectDialog dialog(m_store.get(), projects);
        dialog.show();
        auto *tree = dialog.findChild<QTreeWidget *>();
        QVERIFY(tree);
        QTRY_VERIFY(tree->topLevelItemCount() > 0);
        QVERIFY(!dialog.typeIds().isEmpty());
        // Types were found, but the whole folder cannot take them.
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
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

    void lowDisksShowInTheStatusBar()
    {
        auto *low = m_window->findChild<QLabel *>(QStringLiteral("lowSpace"));
        QVERIFY(low);
        Volume big{QStringLiteral("/big"), QStringLiteral("Big"), 500LL * 1000 * 1000 * 1000,
                   1000LL * 1000 * 1000 * 1000};
        Volume small{QStringLiteral("/small"), QStringLiteral("Small"), 1000 * 1000,
                     100LL * 1000 * 1000 * 1000};
        m_monitor->setVolumesForDisplay({big});
        QVERIFY(low->isHidden());

        m_monitor->setVolumesForDisplay({big, small});
        QVERIFY(!low->isHidden());
        QVERIFY(low->text().contains(QStringLiteral("Small")));
        QVERIFY(!low->text().contains(QStringLiteral("Big")));

        // The dialog lists every disk and follows the monitor.
        FreeSpaceDialog dialog(m_store.get(), m_monitor.get());
        auto *tree = dialog.findChild<QTreeWidget *>();
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 2);
        QVERIFY(tree->topLevelItem(1)->text(0).contains(QStringLiteral("Small")));
        QVERIFY(tree->topLevelItem(0)->foreground(0) != tree->topLevelItem(1)->foreground(0));
        m_monitor->setVolumesForDisplay({big});
        QCOMPARE(tree->topLevelItemCount(), 1);
        QVERIFY(low->isHidden());
    }

private:
    // The window's menu action with this text and shortcut.
    QAction *findAction(const QString &text, const QKeySequence &shortcut) const
    {
        for (QAction *a : m_window->findChildren<QAction *>()) {
            if (a->text() == text && a->shortcut() == shortcut)
                return a;
        }
        return nullptr;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Store> m_store;
    std::unique_ptr<Monitor> m_monitor;
    std::unique_ptr<MainWindow> m_window;
    FolderListView *m_list = nullptr;
    GroupListView *m_groups = nullptr;
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
