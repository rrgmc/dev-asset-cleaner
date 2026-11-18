#include "KnownCaches.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("KnownCaches", text);
}

QString home(const QString &relative)
{
    return QDir::home().filePath(relative);
}

QString env(const char *name)
{
    return QDir::fromNativeSeparators(qEnvironmentVariable(name));
}

// %LOCALAPPDATA% on Windows; empty elsewhere.
QString localAppData(const QString &relative)
{
    const QString base = env("LOCALAPPDATA");
    return base.isEmpty() ? QString() : QDir(base).filePath(relative);
}

} // namespace

namespace KnownCaches {

QList<KnownCache> find()
{
    // Each cache lists its candidate folders; the first that exists wins.
    struct Candidate
    {
        QString name;
        QStringList paths;
    };
    const QString goPath = env("GOPATH").section(QDir::listSeparator(), 0, 0);
    const QList<Candidate> candidates = {
        {tr("Go module cache"),
         {env("GOMODCACHE"), goPath.isEmpty() ? QString() : goPath + QStringLiteral("/pkg/mod"),
          home(QStringLiteral("go/pkg/mod"))}},
        {tr("Go build cache"),
         {env("GOCACHE"), home(QStringLiteral("Library/Caches/go-build")),
          home(QStringLiteral(".cache/go-build")), localAppData(QStringLiteral("go-build"))}},
        {tr("Cargo registry"),
         {env("CARGO_HOME").isEmpty() ? QString() : env("CARGO_HOME") + QStringLiteral("/registry"),
          home(QStringLiteral(".cargo/registry"))}},
        {tr("npm cache"),
         {home(QStringLiteral(".npm/_cacache")), localAppData(QStringLiteral("npm-cache"))}},
        {tr("Yarn cache"),
         {home(QStringLiteral("Library/Caches/Yarn")), home(QStringLiteral(".cache/yarn")),
          localAppData(QStringLiteral("Yarn/Cache"))}},
        {tr("pnpm store"),
         {home(QStringLiteral("Library/pnpm/store")), home(QStringLiteral(".local/share/pnpm/store")),
          localAppData(QStringLiteral("pnpm/store"))}},
        {tr("pip cache"),
         {home(QStringLiteral("Library/Caches/pip")), home(QStringLiteral(".cache/pip")),
          localAppData(QStringLiteral("pip/Cache"))}},
        {tr("Gradle caches"), {home(QStringLiteral(".gradle/caches"))}},
        {tr("Maven repository"), {home(QStringLiteral(".m2/repository"))}},
        {tr("NuGet packages"), {home(QStringLiteral(".nuget/packages"))}},
        {tr("ccache"),
         {env("CCACHE_DIR"), home(QStringLiteral("Library/Caches/ccache")),
          home(QStringLiteral(".cache/ccache")), home(QStringLiteral(".ccache")),
          localAppData(QStringLiteral("ccache"))}},
        {tr("Xcode derived data"), {home(QStringLiteral("Library/Developer/Xcode/DerivedData"))}},
        {tr("Homebrew downloads"), {home(QStringLiteral("Library/Caches/Homebrew"))}},
    };

    QList<KnownCache> found;
    for (const Candidate &c : candidates) {
        for (const QString &path : c.paths) {
            if (!path.isEmpty() && QFileInfo(path).isDir()) {
                found.append(KnownCache{c.name, QDir::cleanPath(path)});
                break;
            }
        }
    }
    return found;
}

} // namespace KnownCaches
