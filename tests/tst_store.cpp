#include "model/Store.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestStore : public QObject
{
    Q_OBJECT

private slots:
    void missingFileStartsWithDefaultGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Store store(dir.filePath(QStringLiteral("data.json")));
        QVERIFY(store.load());
        QVERIFY(store.isNew());
        QVERIFY(store.folders().isEmpty());
        QCOMPARE(store.groups().size(), 1);
        QCOMPARE(store.groups().first().name, Store::defaultGroupName());
        QCOMPARE(store.settings(), Settings{});
    }

    void versionOneFolders()
    {
        // Before groups, the folders sat at the top level. They go to a "Default" group.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(R"({"version":1,"folders":[{"name":"a","path":"/a"},{"name":"b","path":"/b"}]})");
        out.close();

        Store store(file);
        QVERIFY(store.load());
        QCOMPARE(store.groups().size(), 1);
        QCOMPARE(store.groups().first().name, Store::defaultGroupName());
        QCOMPARE(store.folders().size(), 2);
        QCOMPARE(store.folders().at(0).groupId, store.groups().first().id);
        QCOMPARE(store.folders().at(1).name, QStringLiteral("b"));
    }

    void oldWarningLimits_data()
    {
        QTest::addColumn<QByteArray>("settings");
        QTest::addColumn<Alert>("clean");
        QTest::addColumn<Alert>("free");
        QTest::newRow("defaults")
            << QByteArray(R"({"freeWarnBytes":10737418240,"totalWarnBytes":0})")
            << Settings::defaultCleanAlert() << Settings::defaultFreeAlert();
        QTest::newRow("own limits")
            << QByteArray(R"({"freeWarnBytes":2000000000,"totalWarnBytes":30000000000})")
            << Alert{{30'000'000'000LL, 0, 0}, Severity::Low}
            << Alert{{2'000'000'000LL, 0, 0}, Severity::Low};
        QTest::newRow("free off")
            << QByteArray(R"({"freeWarnBytes":0})") << Settings::defaultCleanAlert()
            << Alert{{0, 0, 0}, Severity::Low};
    }

    void oldWarningLimits()
    {
        // Before the three levels, each warning had one limit. One of the user's own becomes
        // the Low level; the old defaults give way to the new levels.
        QFETCH(QByteArray, settings);
        QFETCH(Alert, clean);
        QFETCH(Alert, free);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(R"({"version":3,"settings":)" + settings + R"(,"groups":[]})");
        out.close();

        Store store(file);
        QVERIFY(store.load());
        QCOMPARE(store.settings().cleanAlert, clean);
        QCOMPARE(store.settings().freeAlert, free);
        QVERIFY(store.settings().freeAlertOffDisks.isEmpty());
    }

    void alertLevels()
    {
        const Alert clean{{10, 20, 30}, Severity::Medium};
        QCOMPARE(clean.levelFor(10, Alert::Direction::Above), Severity::None);
        QCOMPARE(clean.levelFor(11, Alert::Direction::Above), Severity::Low);
        QCOMPARE(clean.levelFor(25, Alert::Direction::Above), Severity::Medium);
        QCOMPARE(clean.levelFor(31, Alert::Direction::Above), Severity::High);
        QVERIFY(clean.orderProblem(Alert::Direction::Above).isEmpty());
        QVERIFY(!clean.orderProblem(Alert::Direction::Below).isEmpty());

        // A level that is off is skipped; the next one starts from the one before it.
        const Alert free{{30, 0, 10}, Severity::Low};
        QCOMPARE(free.levelFor(30, Alert::Direction::Below), Severity::None);
        QCOMPARE(free.levelFor(29, Alert::Direction::Below), Severity::Low);
        QCOMPARE(free.levelFor(9, Alert::Direction::Below), Severity::High);
        QVERIFY(free.orderProblem(Alert::Direction::Below).isEmpty());
        QVERIFY(!Alert({{10, 10, 0}, Severity::Low}).orderProblem(Alert::Direction::Above).isEmpty());
        QCOMPARE(Alert{}.levelFor(1'000'000, Alert::Direction::Above), Severity::None);
    }

    void noGroupsStaysEmpty()
    {
        // Deleting every group is kept: "Default" comes back only for a new file.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        {
            Store store(file);
            store.setAutoSave(false);
            QVERIFY(store.load());
            store.removeGroup(store.groups().first().id);
            QVERIFY(store.save());
        }
        Store loaded(file);
        QVERIFY(loaded.load());
        QVERIFY(loaded.groups().isEmpty());
        Folder f;
        f.path = QStringLiteral("/a");
        QVERIFY(loaded.addFolder(f).isNull());
        QVERIFY(loaded.folders().isEmpty());
    }

    void roundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));

        CleanerType custom;
        custom.id = QStringLiteral("custom-1");
        custom.name = QStringLiteral("Make");
        custom.rules = {CleanRule{QStringLiteral("build"), {QStringLiteral("Makefile")}}};

        Folder f;
        f.name = QStringLiteral("Projects");
        f.path = QStringLiteral("/somewhere/projects");
        f.typeIds = {QStringLiteral("rust"), QStringLiteral("custom-1")};
        f.warnBytes = 5'000'000'000LL;
        f.excludes = {QStringLiteral("keep"), QStringLiteral("work in progress")};
        f.inWarnings = false;

        Settings settings;
        settings.closeToTray = false;
        settings.rescanMinutes = 5;
        settings.freeAlert = Alert{{3'000'000'000LL, 0, 1'000'000'000LL}, Severity::High};
        settings.cleanAlert = Alert{{99'000'000'000LL, 120'000'000'000LL, 0}, Severity::None};
        settings.freeAlertOffDisks = {QStringLiteral("/mnt/backup")};
        settings.deleteDirectly = true;

        Folder other;
        other.name = QStringLiteral("Cache");
        other.path = QStringLiteral("/somewhere/cache");

        {
            Store store(file);
            store.setAutoSave(false);
            QVERIFY(store.load());
            const QUuid tools = store.addGroup(QStringLiteral("Tools"));
            f.groupId = tools;
            other.groupId = store.groups().first().id;
            store.addFolder(f);
            store.addFolder(other);
            store.setCustomTypes({custom});
            store.setSettings(settings);
            QVERIFY(store.save());
        }

        Store loaded(file);
        QVERIFY(loaded.load());
        QVERIFY(!loaded.isNew());
        QCOMPARE(loaded.groups().size(), 2);
        QCOMPARE(loaded.groups().at(0).name, Store::defaultGroupName());
        QCOMPARE(loaded.groups().at(1).name, QStringLiteral("Tools"));
        QCOMPARE(loaded.folders().size(), 2);
        const QList<Folder> tools = loaded.foldersIn({loaded.groups().at(1).id});
        QCOMPARE(tools.size(), 1);
        const Folder &g = tools.first();
        QCOMPARE(g.groupId, loaded.groups().at(1).id);
        QCOMPARE(loaded.foldersIn({loaded.groups().at(0).id}).first().name, other.name);
        QCOMPARE(g.name, f.name);
        QCOMPARE(g.path, f.path);
        QCOMPARE(g.typeIds, f.typeIds);
        QCOMPARE(g.warnBytes, f.warnBytes);
        QCOMPARE(g.excludes, f.excludes);
        QCOMPARE(g.inWarnings, false);
        QVERIFY(loaded.foldersIn({loaded.groups().at(0).id}).first().inWarnings);
        QVERIFY(loaded.foldersIn({loaded.groups().at(0).id}).first().excludes.isEmpty());
        QCOMPARE(loaded.customTypes().size(), 1);
        QCOMPARE(loaded.customTypes().first(), custom);
        QCOMPARE(loaded.settings(), settings);

        const QList<CleanerType> types = loaded.typesFor(g);
        QCOMPARE(types.size(), 2);
        QCOMPARE(types.at(0).id, QStringLiteral("rust"));
        QCOMPARE(types.at(1).id, QStringLiteral("custom-1"));
    }

    void unknownTypesAreDropped()
    {
        // Never a fallback to deleting everything.
        Store store(QStringLiteral("unused.json"));
        Folder f;
        f.typeIds = {QStringLiteral("gone")};
        QVERIFY(store.typesFor(f).isEmpty());
        f.typeIds = {QStringLiteral("rust")};
        f.kind = Folder::Kind::Whole;
        QVERIFY(store.typesFor(f).isEmpty());
    }

    void customTypeCannotReplaceBuiltIn()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(R"({"types":[{"id":"rust","name":"Evil","rules":[{"pattern":"*"}]}]})");
        out.close();

        Store store(file);
        QVERIFY(store.load());
        QVERIFY(store.customTypes().isEmpty());
        QCOMPARE(store.type(QStringLiteral("rust"))->name, QStringLiteral("Rust"));
    }

    void oldContentsTypesBecomeWholeFolders()
    {
        // Version 2 had "Folder contents" as a type, built in or custom.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(R"({"version":2,
            "types":[{"id":"mine","name":"Mine","mode":"contents"},
                     {"id":"make","name":"Make","mode":"rules","rules":[{"pattern":"build"}]}],
            "groups":[{"name":"g","folders":[
                {"name":"builtin","path":"/a","types":["contents"]},
                {"name":"custom","path":"/b","types":["mine"],"excludes":["keep"]},
                {"name":"none","path":"/c","types":[]},
                {"name":"rules","path":"/d","types":["rust","make"]},
                {"name":"future","path":"/e","kind":"something-new","types":["rust"]}]}]})");
        out.close();

        Store store(file);
        QVERIFY(store.load());
        QCOMPARE(store.customTypes().size(), 1);
        QCOMPARE(store.customTypes().first().id, QStringLiteral("make"));
        const QList<Folder> folders = store.folders();
        QCOMPARE(folders.size(), 5);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(folders.at(i).kind, Folder::Kind::Whole);
            QVERIFY(folders.at(i).typeIds.isEmpty());
        }
        QCOMPARE(folders.at(1).excludes, QStringList{QStringLiteral("keep")});
        QCOMPARE(folders.at(3).kind, Folder::Kind::Matching);
        QCOMPARE(folders.at(3).typeIds, (QStringList{QStringLiteral("rust"), QStringLiteral("make")}));
        // A kind this version does not know deletes no more than its types match.
        QCOMPARE(folders.at(4).kind, Folder::Kind::Matching);

        // Saved again, the kinds are kept and the old type is gone.
        QVERIFY(store.save());
        Store again(file);
        QVERIFY(again.load());
        QCOMPARE(again.folders().at(0).kind, Folder::Kind::Whole);
        QCOMPARE(again.folders().at(3).kind, Folder::Kind::Matching);
    }

    void brokenFileIsAnError()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("{ not json");
        out.close();

        Store store(file);
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
    }

    void moveAndRemove()
    {
        Store store(QStringLiteral("unused.json"));
        store.setAutoSave(false);
        const QUuid g = store.addGroup(QStringLiteral("g"));
        QSignalSpy changed(&store, &Store::changed);
        const QUuid a = store.addFolder(Folder{{}, QStringLiteral("a"), QStringLiteral("/a"), {}, 0, g});
        const QUuid b = store.addFolder(Folder{{}, QStringLiteral("b"), QStringLiteral("/b"), {}, 0, g});
        store.moveBy(b, -1);
        QCOMPARE(store.folders().first().id, b);
        store.moveBy(b, -1);  // already first
        QCOMPARE(changed.count(), 3);
        QCOMPARE(store.folderWithPath(QStringLiteral("/A"))->id, a);
        store.removeFolder(a);
        QCOMPARE(store.folders().size(), 1);
        QVERIFY(!store.folder(a));
    }

    void moveNextTo()
    {
        Store store(QStringLiteral("unused.json"));
        store.setAutoSave(false);
        const QUuid g1 = store.addGroup(QStringLiteral("one"));
        const QUuid g2 = store.addGroup(QStringLiteral("two"));
        const QUuid g3 = store.addGroup(QStringLiteral("three"));
        const auto groupIds = [&store] {
            QList<QUuid> ids;
            for (const Group &g : store.groups())
                ids.append(g.id);
            return ids;
        };
        store.moveGroupNextTo(g1, g3, true);
        QCOMPARE(groupIds(), (QList<QUuid>{g2, g3, g1}));
        store.moveGroupNextTo(g1, g2, false);
        QCOMPARE(groupIds(), (QList<QUuid>{g1, g2, g3}));
        store.moveGroupNextTo(g3, g2, false);
        QCOMPARE(groupIds(), (QList<QUuid>{g1, g3, g2}));
        QSignalSpy changed(&store, &Store::changed);
        store.moveGroupNextTo(g3, g1, true);  // already there
        QCOMPARE(changed.count(), 0);

        const QUuid a = store.addFolder(Folder{{}, QStringLiteral("a"), QStringLiteral("/a"), {}, 0, g1});
        const QUuid x = store.addFolder(Folder{{}, QStringLiteral("x"), QStringLiteral("/x"), {}, 0, g2});
        const QUuid b = store.addFolder(Folder{{}, QStringLiteral("b"), QStringLiteral("/b"), {}, 0, g1});
        const QUuid c = store.addFolder(Folder{{}, QStringLiteral("c"), QStringLiteral("/c"), {}, 0, g1});
        const auto folderIds = [&store, g1] {
            QList<QUuid> ids;
            for (const Folder &f : store.foldersIn({g1}))
                ids.append(f.id);
            return ids;
        };
        store.moveNextTo(c, a, false);
        QCOMPARE(folderIds(), (QList<QUuid>{c, a, b}));
        store.moveNextTo(c, b, true);
        QCOMPARE(folderIds(), (QList<QUuid>{a, b, c}));
        // Not next to another group's folder.
        store.moveNextTo(a, x, false);
        QCOMPARE(folderIds(), (QList<QUuid>{a, b, c}));
        QCOMPARE(store.folder(a)->groupId, g1);
    }

    void groups()
    {
        Store store(QStringLiteral("unused.json"));
        store.setAutoSave(false);
        const QUuid g1 = store.addGroup(QStringLiteral("one"));
        const QUuid g2 = store.addGroup(QStringLiteral("two"));
        const QUuid a = store.addFolder(Folder{{}, QStringLiteral("a"), QStringLiteral("/a"), {}, 0, g1});
        const QUuid x = store.addFolder(Folder{{}, QStringLiteral("x"), QStringLiteral("/x"), {}, 0, g2});
        const QUuid b = store.addFolder(Folder{{}, QStringLiteral("b"), QStringLiteral("/b"), {}, 0, g1});

        // Moving within a group passes over the other groups' folders.
        store.moveBy(b, -1);
        QCOMPARE(store.foldersIn({g1}).first().id, b);
        QCOMPARE(store.foldersIn({g2}).first().id, x);

        // Folders come group by group, in the groups' order.
        store.moveGroupBy(g2, -1);
        QCOMPARE(store.groups().first().id, g2);
        const QList<Folder> both = store.foldersIn({g1, g2});
        QCOMPARE(both.size(), 3);
        QCOMPARE(both.at(0).id, x);

        // Editing a folder keeps its group; moving it changes it.
        Folder edited = *store.folder(a);
        edited.groupId = QUuid::createUuid();
        store.updateFolder(edited);
        QCOMPARE(store.folder(a)->groupId, g1);
        store.moveToGroup(a, g2);
        QCOMPARE(store.foldersIn({g2}).last().id, a);

        store.renameGroup(g1, QStringLiteral("first"));
        QCOMPARE(store.group(g1)->name, QStringLiteral("first"));

        // Removing a group stops monitoring its folders.
        store.removeGroup(g2);
        QVERIFY(!store.group(g2));
        QVERIFY(!store.folder(a));
        QVERIFY(!store.folder(x));
        QCOMPARE(store.folders().size(), 1);
        QCOMPARE(store.folders().first().id, b);
    }

    void allLanguagesHasEveryRule()
    {
        const CleanerType *all = nullptr;
        for (const CleanerType &t : BuiltInTypes::all()) {
            if (t.id == QLatin1String("all-languages"))
                all = &t;
        }
        QVERIFY(all);
        for (const CleanerType &t : BuiltInTypes::all()) {
            for (const CleanRule &r : t.rules)
                QVERIFY2(all->rules.contains(r), qPrintable(t.name + QLatin1Char(' ') + r.pattern));
        }
    }
};

QTEST_GUILESS_MAIN(TestStore)
#include "tst_store.moc"
