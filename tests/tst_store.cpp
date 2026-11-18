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
    void missingFileStartsEmpty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Store store(dir.filePath(QStringLiteral("data.json")));
        QVERIFY(store.load());
        QVERIFY(store.isNew());
        QVERIFY(store.folders().isEmpty());
        QCOMPARE(store.settings(), Settings{});
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

        Settings settings;
        settings.closeToTray = false;
        settings.rescanMinutes = 5;
        settings.freeWarnBytes = 1'000'000'000LL;
        settings.totalWarnBytes = 99'000'000'000LL;

        {
            Store store(file);
            store.setAutoSave(false);
            QVERIFY(store.load());
            store.addFolder(f);
            store.setCustomTypes({custom});
            store.setSettings(settings);
            QVERIFY(store.save());
        }

        Store loaded(file);
        QVERIFY(loaded.load());
        QVERIFY(!loaded.isNew());
        QCOMPARE(loaded.folders().size(), 1);
        const Folder &g = loaded.folders().first();
        QCOMPARE(g.name, f.name);
        QCOMPARE(g.path, f.path);
        QCOMPARE(g.typeIds, f.typeIds);
        QCOMPARE(g.warnBytes, f.warnBytes);
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
        QSignalSpy changed(&store, &Store::changed);
        const QUuid a = store.addFolder(Folder{{}, QStringLiteral("a"), QStringLiteral("/a"), {}, 0});
        const QUuid b = store.addFolder(Folder{{}, QStringLiteral("b"), QStringLiteral("/b"), {}, 0});
        store.moveBy(b, -1);
        QCOMPARE(store.folders().first().id, b);
        store.moveBy(b, -1);  // already first
        QCOMPARE(changed.count(), 3);
        QCOMPARE(store.folderWithPath(QStringLiteral("/A"))->id, a);
        store.removeFolder(a);
        QCOMPARE(store.folders().size(), 1);
        QVERIFY(!store.folder(a));
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
