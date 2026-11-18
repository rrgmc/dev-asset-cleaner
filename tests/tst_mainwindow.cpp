#include "clean/Monitor.h"
#include "model/Store.h"
#include "ui/DetectDialog.h"
#include "ui/FolderDelegate.h"
#include "ui/FolderListView.h"
#include "ui/FolderModel.h"
#include "ui/MainWindow.h"
#include "ui/SystemFoldersDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
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
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")));
        QCOMPARE(m_store->folders().size(), 2);
        QCOMPARE(m_store->folders().at(1).typeIds, QStringList{BuiltInTypes::contentsId()});

        const QUuid projects = m_store->folders().at(0).id;
        QTRY_VERIFY(m_monitor->result(projects).valid);
        QCOMPARE(m_monitor->result(projects).cleanableBytes, 4000);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 4700);
        QCOMPARE(m_monitor->totalBytes(), 4710);

        // Adding the same folder again selects it instead.
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")));
        QCOMPARE(m_store->folders().size(), 2);
        QCOMPARE(m_list->currentId(), projects);
    }

    void cleanButtonCleansOneFolder()
    {
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")));
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
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")));
        QTRY_COMPARE(m_monitor->cleanableBytes(), 4700);
        QTest::keyClick(m_window.get(), Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(m_monitor->cleanableBytes(), 0);
        QTRY_VERIFY(!m_monitor->isBusy());
        QVERIFY(QFileInfo(m_dir->filePath(QStringLiteral("cache"))).isDir());
        QVERIFY(!QFileInfo::exists(m_dir->filePath(QStringLiteral("cache/blob"))));
    }

    void filterHidesOtherFolders()
    {
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("projects")), {QStringLiteral("rust")});
        m_window->addFolderPath(m_dir->filePath(QStringLiteral("cache")));
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
    }

    void systemFoldersDialogSkipsListedFolders()
    {
        const QString cache = QDir::cleanPath(m_dir->filePath(QStringLiteral("cache")));
        const QString trash = QDir::cleanPath(m_dir->filePath(QStringLiteral("trash")));
        QVERIFY(QDir().mkpath(trash));
        m_window->addFolderPath(cache);
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
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
