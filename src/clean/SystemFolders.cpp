#include "SystemFolders.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#else
#include <unistd.h>
#endif

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("SystemFolders", text);
}

[[maybe_unused]] QString home(const QString &relative)
{
    return QDir::home().filePath(relative);
}

// A volume's root without the trailing separator, as shown to the user: "C:" or "/Volumes/USB".
[[maybe_unused]] QString volumeLabel(const QStorageInfo &volume)
{
    QString root = QDir::toNativeSeparators(volume.rootPath());
    while (root.size() > 1 && (root.endsWith(QLatin1Char('\\')) || root.endsWith(QLatin1Char('/'))))
        root.chop(1);
    return root;
}

#ifdef Q_OS_WIN
// The current user's security identifier, such as S-1-5-21-…-1001; each drive's Recycle Bin
// keeps the user's files in a folder with this name.
QString userSid()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    QString result;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    QByteArray buffer(static_cast<qsizetype>(size), '\0');
    if (size > 0 && GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
        LPWSTR text = nullptr;
        const auto *user = reinterpret_cast<const TOKEN_USER *>(buffer.constData());
        if (ConvertSidToStringSidW(user->User.Sid, &text)) {
            result = QString::fromWCharArray(text);
            LocalFree(text);
        }
    }
    CloseHandle(token);
    return result;
}

// %LOCALAPPDATA%\relative.
QString localAppData(const QString &relative)
{
    const QString base = QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA"));
    return base.isEmpty() ? QString() : QDir(base).filePath(relative);
}
#endif

QList<SystemFolder> candidates()
{
    QList<SystemFolder> list;
#ifdef Q_OS_WIN
    if (const QString sid = userSid(); !sid.isEmpty()) {
        for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
            if (!volume.isValid() || !volume.isReady() || volume.isReadOnly())
                continue;
            list.append({tr("Recycle Bin (%1)").arg(volumeLabel(volume)),
                         QDir(volume.rootPath()).filePath(QStringLiteral("$Recycle.Bin/") + sid),
                         tr("Files you deleted on this drive. Emptying it deletes them for good.")});
        }
    }
    const QString reports = tr("Reports Windows keeps about programs that stopped working.");
    list.append({tr("Windows error reports"),
                 localAppData(QStringLiteral("Microsoft/Windows/WER/ReportArchive")), reports});
    list.append({tr("Windows error reports (queued)"),
                 localAppData(QStringLiteral("Microsoft/Windows/WER/ReportQueue")), reports});
    list.append({tr("Crash dumps"), localAppData(QStringLiteral("CrashDumps")),
                 tr("Memory dumps of programs that crashed.")});
    const QString shaders = tr("Compiled graphics shaders. Games and programs compile them again "
                               "when they need them.");
    list.append({tr("DirectX shader cache"), localAppData(QStringLiteral("D3DSCache")), shaders});
    list.append({tr("NVIDIA DirectX shader cache"), localAppData(QStringLiteral("NVIDIA/DXCache")),
                 shaders});
    list.append({tr("NVIDIA OpenGL shader cache"), localAppData(QStringLiteral("NVIDIA/GLCache")),
                 shaders});
    list.append({tr("AMD DirectX shader cache"), localAppData(QStringLiteral("AMD/DxCache")),
                 shaders});
#else
    const QString emptying = tr("Emptying it deletes them for good.");
#ifdef Q_OS_MACOS
    list.append({tr("Trash"), home(QStringLiteral(".Trash")),
                 tr("Files you moved to the Trash. %1 Reading it needs Full Disk Access in "
                    "System Settings > Privacy & Security.")
                     .arg(emptying)});
    // Other volumes keep the user's trash in .Trashes/<user id>.
    const QString uid = QString::number(getuid());
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        if (!volume.isValid() || !volume.isReady() || volume.isReadOnly()
            || !volume.rootPath().startsWith(QStringLiteral("/Volumes/")))
            continue;
        const QString name = volume.name().isEmpty() ? volumeLabel(volume) : volume.name();
        list.append({tr("Trash on %1").arg(name),
                     QDir(volume.rootPath()).filePath(QStringLiteral(".Trashes/") + uid),
                     tr("Files you moved to the Trash from this volume. %1").arg(emptying)});
    }
    list.append({tr("Crash reports"), home(QStringLiteral("Library/Logs/DiagnosticReports")),
                 tr("Reports macOS keeps about programs that crashed or hung.")});
#endif
    // The freedesktop.org trash, used on Linux and by some command-line tools elsewhere.
    const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    list.append({tr("Trash (freedesktop.org)"),
                 dataHome.isEmpty() ? home(QStringLiteral(".local/share/Trash"))
                                    : QDir(dataHome).filePath(QStringLiteral("Trash")),
                 tr("Files moved to the trash by desktop environments and command-line tools. "
                    "%1")
                     .arg(emptying)});
    const QString cacheHome = qEnvironmentVariable("XDG_CACHE_HOME");
    list.append({tr("Thumbnails"),
                 cacheHome.isEmpty() ? home(QStringLiteral(".cache/thumbnails"))
                                     : QDir(cacheHome).filePath(QStringLiteral("thumbnails")),
                 tr("Previews of pictures and documents. File managers make them again.")});
#endif
    return list;
}

} // namespace

namespace SystemFolders {

QList<SystemFolder> find()
{
    QList<SystemFolder> found;
    QSet<QString> seen;
    for (SystemFolder folder : candidates()) {
        if (folder.path.isEmpty())
            continue;
        const QFileInfo info(folder.path);
        // A link could point anywhere, such as at a folder the user keeps.
        if (!info.isDir() || info.isSymbolicLink())
            continue;
#ifdef Q_OS_WIN
        if (info.isJunction())
            continue;
#endif
        folder.path = QDir::cleanPath(QDir::fromNativeSeparators(folder.path));
        if (seen.contains(folder.path))
            continue;
        seen.insert(folder.path);
        found.append(folder);
    }
    return found;
}

} // namespace SystemFolders
