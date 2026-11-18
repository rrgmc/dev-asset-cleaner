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

        Settings settings;
        settings.closeToTray = false;
        settings.rescanMinutes = 5;
        settings.freeWarnBytes = 1'000'000'000LL;
        settings.totalWarnBytes = 99'000'000'000LL;

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
        QVERIFY(loaded.foldersIn({loaded.groups().at(0).id}).first().excludes.isEmpty());
        QCOMPARE(loaded.customTypes().size(), 1);
        QCOMPARE(loaded.customTypes().first(), custom);
        QCOMPARE(loaded.settings(), settings);

        const QList<CleanerType> types = loaded.typesFor(g);
        QCOMPARE(types.size(), 2);
        QCOMPARE(types.at(0).id, QStringLiteral("rust"));
        QCOMPARE(types.at(1).id, QStringLiteral("custom-1"));
    }

    void unknownTypesFallBackToContents()
    {
        Store store(QStringLiteral("unused.json"));
        Folder f;
        f.typeIds = {QStringLiteral("gone")};
        const QList<CleanerType> types = store.typesFor(f);
        QCOMPARE(types.size(), 1);
        QCOMPARE(types.first().id, BuiltInTypes::contentsId());
    }

    void customTypeCannotReplaceBuiltIn()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("data.json"));
        QFile out(file);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(R"({"types":[{"id":"rust","name":"Evil","mode":"contents"}]})");
        out.close();

        Store store(file);
        QVERIFY(store.load());
        QVERIFY(store.customTypes().isEmpty());
        QCOMPARE(store.type(QStringLiteral("rust"))->mode, CleanerType::Mode::Rules);
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
