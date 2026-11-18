#include "clean/Scanner.h"
#include "clean/SystemFolders.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

namespace {

void writeFile(const QString &path, int bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(QByteArray(bytes, 'x'));
}

const CleanerType &builtIn(const char *id)
{
    for (const CleanerType &t : BuiltInTypes::all()) {
        if (t.id == QLatin1String(id))
            return t;
    }
    qFatal("no built-in type %s", id);
}

QStringList relativePaths(const QString &base, const QList<ScanItem> &items)
{
    QStringList paths;
    for (const ScanItem &item : items)
        paths.append(QDir(base).relativeFilePath(item.path));
    paths.sort();
    return paths;
}

} // namespace

class TestScanner : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        // A folder of projects in several languages.
        writeFile(path("rustproj/Cargo.toml"), 10);
        writeFile(path("rustproj/src/main.rs"), 20);
        writeFile(path("rustproj/target/debug/app"), 1000);
        writeFile(path("rustproj/target/debug/deps/lib.rlib"), 500);
        writeFile(path("other/target/keep.txt"), 40);  // no Cargo.toml: not Rust
        writeFile(path("web/package.json"), 5);
        writeFile(path("web/node_modules/a/index.js"), 300);
        writeFile(path("web/node_modules/a/node_modules/b/index.js"), 70);
        writeFile(path("deep/er/cmake/CMakeLists.txt"), 3);
        writeFile(path("deep/er/cmake/cmake-build-debug/out.o"), 200);
    }

    void cleanup()
    {
        // Read-only folders left by a failed test would stop QTemporaryDir from removing them.
        Scanner::removeTree(m_dir->path());
        m_dir.reset();
    }

    void rulesFindOnlyMatchingFolders()
    {
        const ScanResult r = Scanner::scan(m_dir->path(), {builtIn("rust")});
        QVERIFY(r.valid);
        QVERIFY(r.error.isEmpty());
        QCOMPARE(relativePaths(m_dir->path(), r.items), QStringList{QStringLiteral("rustproj/target")});
        QCOMPARE(r.cleanableBytes, 1500);
        QCOMPARE(r.totalBytes, 10 + 20 + 1500 + 40 + 5 + 300 + 70 + 3 + 200);
    }

    void severalTypesAndNestedMatches()
    {
        const ScanResult r =
            Scanner::scan(m_dir->path(), {builtIn("rust"), builtIn("node"), builtIn("cpp")});
        // A match is not searched further: the nested node_modules is part of the outer one.
        QCOMPARE(relativePaths(m_dir->path(), r.items),
                 (QStringList{QStringLiteral("deep/er/cmake/cmake-build-debug"),
                              QStringLiteral("rustproj/target"), QStringLiteral("web/node_modules")}));
        QCOMPARE(r.cleanableBytes, 1500 + 370 + 200);
        // Largest first.
        QCOMPARE(r.items.first().bytes, 1500);
    }

    void markersUseWildcards()
    {
        CleanerType t;
        t.rules = {CleanRule{QStringLiteral("TARGET"), {QStringLiteral("*.toml")}}};
        const ScanResult r = Scanner::scan(m_dir->path(), {t});
        QCOMPARE(relativePaths(m_dir->path(), r.items), QStringList{QStringLiteral("rustproj/target")});
    }

    void contentsTakesEverything()
    {
        const ScanResult r = Scanner::scan(path("web"), {builtIn("contents"), builtIn("rust")});
        QCOMPARE(relativePaths(path("web"), r.items),
                 (QStringList{QStringLiteral("node_modules"), QStringLiteral("package.json")}));
        QCOMPARE(r.cleanableBytes, 375);
        QCOMPARE(r.totalBytes, 375);
    }

    void missingFolderIsAnError()
    {
        const ScanResult r = Scanner::scan(path("nope"), {builtIn("contents")});
        QVERIFY(r.valid);
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.items.isEmpty());
    }

    void cancelStopsTheScan()
    {
        const ScanResult r = Scanner::scan(m_dir->path(), {builtIn("rust")}, [] { return true; });
        QVERIFY(!r.valid);
    }

    void cleanDeletesOnlyTheItems()
    {
        const ScanResult r = Scanner::scan(m_dir->path(), {builtIn("rust"), builtIn("node")});
        const CleanResult c = Scanner::clean(r.items);
        QVERIFY2(c.failures.isEmpty(), qPrintable(c.failures.join(QLatin1Char('\n'))));
        QCOMPARE(c.freedBytes, 1870);
        QVERIFY(!QFileInfo::exists(path("rustproj/target")));
        QVERIFY(!QFileInfo::exists(path("web/node_modules")));
        QVERIFY(QFileInfo::exists(path("rustproj/Cargo.toml")));
        QVERIFY(QFileInfo::exists(path("other/target/keep.txt")));

        const ScanResult after = Scanner::scan(m_dir->path(), {builtIn("rust"), builtIn("node")});
        QCOMPARE(after.cleanableBytes, 0);
    }

    void cleanContentsKeepsTheFolder()
    {
        const ScanResult r = Scanner::scan(path("web"), {builtIn("contents")});
        const CleanResult c = Scanner::clean(r.items);
        QVERIFY(c.failures.isEmpty());
        QVERIFY(QFileInfo(path("web")).isDir());
        QVERIFY(QDir(path("web")).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot));
    }

    void removesReadOnlyTrees()
    {
        // Like Go's module cache: read-only files in read-only folders.
        const QString file = path("modcache/example.com/mod@v1/file.go");
        writeFile(file, 12);
        const auto readOnlyFile = QFileDevice::ReadOwner | QFileDevice::ReadUser;
        const auto readOnlyDir = readOnlyFile | QFileDevice::ExeOwner | QFileDevice::ExeUser;
        QVERIFY(QFile::setPermissions(file, readOnlyFile));
        QVERIFY(QFile::setPermissions(path("modcache/example.com/mod@v1"), readOnlyDir));
        QVERIFY(QFile::setPermissions(path("modcache/example.com"), readOnlyDir));

        const ScanResult r = Scanner::scan(path("modcache"), {builtIn("contents")});
        QCOMPARE(r.cleanableBytes, 12);
        const CleanResult c = Scanner::clean(r.items);
        QVERIFY2(c.failures.isEmpty(), qPrintable(c.failures.join(QLatin1Char('\n'))));
        QVERIFY(!QFileInfo::exists(path("modcache/example.com")));
    }

    void linksAreNotFollowed()
    {
#ifdef Q_OS_WIN
        QSKIP("QFile::link makes .lnk shortcuts on Windows");
#else
        writeFile(path("outside/precious.txt"), 64);
        QVERIFY(QFile::link(path("outside"), path("rustproj/target/link")));
        const ScanResult r = Scanner::scan(m_dir->path(), {builtIn("rust")});
        QCOMPARE(r.cleanableBytes, 1500);
        const CleanResult c = Scanner::clean(r.items);
        QVERIFY(c.failures.isEmpty());
        QVERIFY(!QFileInfo::exists(path("rustproj/target")));
        QVERIFY(QFileInfo::exists(path("outside/precious.txt")));
#endif
    }

    void detectFindsEachType()
    {
        writeFile(path("deep/er/cmake/build/obj.o"), 9);  // CMake's build next to CMakeLists.txt
        const DetectResult r = Scanner::detect(m_dir->path(), BuiltInTypes::all());
        QVERIFY(r.valid);
        QVERIFY(r.error.isEmpty());
        QStringList ids;
        for (const Detection &d : r.detections)
            ids.append(d.typeId);
        // Largest first; "Folder contents" is never detected.
        QCOMPARE(ids, (QStringList{QStringLiteral("all-languages"), QStringLiteral("rust"),
                                   QStringLiteral("node"), QStringLiteral("cpp")}));
        QCOMPARE(r.detections.at(1).bytes, 1500);
        QCOMPARE(r.detections.at(2).bytes, 370);
        QCOMPARE(r.detections.at(3).bytes, 209);
        QCOMPARE(r.detections.at(3).items.size(), 2);
        QCOMPARE(r.cleanableBytes, 1500 + 370 + 209);
        QCOMPARE(r.totalBytes, 10 + 20 + 1500 + 40 + 5 + 300 + 70 + 3 + 200 + 9);
    }

    void detectCountsSharedFoldersForEachType()
    {
        // A build folder next to both CMakeLists.txt and build.gradle.
        writeFile(path("both/CMakeLists.txt"), 1);
        writeFile(path("both/build.gradle"), 1);
        writeFile(path("both/build/out"), 50);
        const DetectResult r = Scanner::detect(path("both"), {builtIn("cpp"), builtIn("jvm")});
        QCOMPARE(r.detections.size(), 2);
        QCOMPARE(r.detections.at(0).bytes, 50);
        QCOMPARE(r.detections.at(1).bytes, 50);
        QCOMPARE(r.cleanableBytes, 50);
    }

    void detectNothing()
    {
        const DetectResult r = Scanner::detect(path("other"), BuiltInTypes::all());
        QVERIFY(r.valid);
        QVERIFY(r.detections.isEmpty());
        QCOMPARE(r.totalBytes, 40);
        QVERIFY(!Scanner::detect(path("missing"), BuiltInTypes::all()).error.isEmpty());
    }

    void unsafeFolders()
    {
        const QList<CleanerType> contents{builtIn("contents")};
        QVERIFY(!Scanner::unsafeReason(QDir::homePath(), contents).isEmpty());
        QVERIFY(!Scanner::unsafeReason(QDir::rootPath(), contents).isEmpty());
        QVERIFY(!Scanner::unsafeReason(QDir(QDir::homePath()).filePath(QStringLiteral("..")), contents)
                     .isEmpty());
        QVERIFY(Scanner::unsafeReason(path("web"), contents).isEmpty());
        // Rules only delete matching subfolders, so any folder is fine.
        QVERIFY(Scanner::unsafeReason(QDir::homePath(), {builtIn("rust")}).isEmpty());
    }

    void systemFoldersAreSafe()
    {
        // Whatever this computer has: each folder exists, is listed once and may be emptied.
        const QList<CleanerType> contents{builtIn("contents")};
        QSet<QString> paths;
        for (const SystemFolder &f : SystemFolders::find()) {
            QVERIFY2(QFileInfo(f.path).isDir(), qPrintable(f.path));
            QVERIFY(!f.name.isEmpty());
            QVERIFY(!f.note.isEmpty());
            QVERIFY2(Scanner::unsafeReason(f.path, contents).isEmpty(), qPrintable(f.path));
            QVERIFY(!paths.contains(f.path));
            paths.insert(f.path);
        }
    }

private:
    QString path(const QString &relative) const { return m_dir->filePath(relative); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

QTEST_GUILESS_MAIN(TestScanner)
#include "tst_scanner.moc"
