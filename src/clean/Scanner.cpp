#include "Scanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>

#include <algorithm>

namespace {

constexpr QDir::Filters AllEntries =
    QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot;

QString tr(const char *text)
{
    return QCoreApplication::translate("Scanner", text);
}

QRegularExpression wildcard(const QString &pattern)
{
    return QRegularExpression(
        QRegularExpression::wildcardToRegularExpression(pattern,
                                                        QRegularExpression::UnanchoredWildcardConversion),
        QRegularExpression::CaseInsensitiveOption);
}

bool anchoredMatch(const QRegularExpression &re, const QString &text)
{
    const QRegularExpressionMatch m = re.match(text);
    return m.hasMatch() && m.capturedStart() == 0 && m.capturedLength() == text.size();
}

// A link that must not be followed: a symbolic link, or a junction on Windows.
bool isLink(const QFileInfo &info)
{
#ifdef Q_OS_WIN
    if (info.isJunction())
        return true;
#endif
    return info.isSymbolicLink();
}

struct CompiledRule
{
    QRegularExpression pattern;
    QList<QRegularExpression> markers;
};

class Walker
{
public:
    Walker(QList<CompiledRule> rules, const CancelCheck &canceled)
        : m_rules(std::move(rules))
        , m_canceled(canceled)
    {
    }

    void walk(const QString &dirPath, ScanResult &result)
    {
        if (m_canceled && m_canceled())
            return;
        const QFileInfoList entries = QDir(dirPath).entryInfoList(AllEntries, QDir::NoSort);
        QStringList names;
        names.reserve(entries.size());
        for (const QFileInfo &e : entries)
            names.append(e.fileName());

        for (const QFileInfo &e : entries) {
            if (isLink(e))
                continue;
            if (!e.isDir()) {
                result.totalBytes += e.size();
                continue;
            }
            if (matches(e.fileName(), names)) {
                const qint64 bytes = Scanner::treeSize(e.filePath(), m_canceled);
                result.items.append(ScanItem{e.filePath(), bytes});
                result.totalBytes += bytes;
                result.cleanableBytes += bytes;
            } else {
                walk(e.filePath(), result);
            }
        }
    }

private:
    bool matches(const QString &name, const QStringList &siblings) const
    {
        for (const CompiledRule &rule : m_rules) {
            if (!anchoredMatch(rule.pattern, name))
                continue;
            if (rule.markers.isEmpty())
                return true;
            for (const QRegularExpression &marker : rule.markers) {
                for (const QString &sibling : siblings) {
                    if (anchoredMatch(marker, sibling))
                        return true;
                }
            }
        }
        return false;
    }

    QList<CompiledRule> m_rules;
    const CancelCheck &m_canceled;
};

bool hasContentsType(const QList<CleanerType> &types)
{
    return std::any_of(types.cbegin(), types.cend(), [](const CleanerType &t) {
        return t.mode == CleanerType::Mode::Contents;
    });
}

void makeWritable(const QString &path, bool isDir)
{
    QFileDevice::Permissions perms = QFile::permissions(path) | QFileDevice::ReadOwner
                                     | QFileDevice::WriteOwner;
    if (isDir)
        perms |= QFileDevice::ExeOwner;
    QFile::setPermissions(path, perms);
}

} // namespace

namespace Scanner {

ScanResult scan(const QString &path, const QList<CleanerType> &types, const CancelCheck &canceled)
{
    ScanResult result;
    const QFileInfo root(path);
    if (!root.exists() || !root.isDir()) {
        result.error = tr("The folder does not exist.");
    } else if (!root.isReadable()) {
        result.error = tr("The folder cannot be read.");
    } else if (hasContentsType(types)) {
        // Everything inside the folder; the other types add nothing to that.
        const QFileInfoList entries = QDir(path).entryInfoList(AllEntries, QDir::NoSort);
        for (const QFileInfo &e : entries) {
            if (canceled && canceled())
                break;
            const qint64 bytes = isLink(e) ? 0 : e.isDir() ? treeSize(e.filePath(), canceled)
                                                           : e.size();
            result.items.append(ScanItem{e.filePath(), bytes});
            result.totalBytes += bytes;
        }
        result.cleanableBytes = result.totalBytes;
    } else {
        QList<CompiledRule> rules;
        for (const CleanerType &t : types) {
            for (const CleanRule &r : t.rules) {
                CompiledRule c{wildcard(r.pattern), {}};
                for (const QString &m : r.markers)
                    c.markers.append(wildcard(m));
                rules.append(c);
            }
        }
        Walker(rules, canceled).walk(path, result);
    }

    std::sort(result.items.begin(), result.items.end(),
              [](const ScanItem &a, const ScanItem &b) { return a.bytes > b.bytes; });
    result.valid = !(canceled && canceled());
    result.finishedAt = QDateTime::currentDateTime();
    return result;
}

qint64 treeSize(const QString &path, const CancelCheck &canceled)
{
    const QFileInfo info(path);
    if (isLink(info))
        return 0;
    if (!info.isDir())
        return info.size();
    qint64 total = 0;
    int count = 0;
    QDirIterator it(path, AllEntries, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo e = it.nextFileInfo();
        if ((++count & 0x3ff) == 0 && canceled && canceled())
            break;
        if (!e.isDir() && !isLink(e))
            total += e.size();
    }
    return total;
}

QString unsafeReason(const QString &path, const QList<CleanerType> &types)
{
    if (!hasContentsType(types))
        return {};
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (clean.isEmpty())
        return tr("The folder has no path.");
    const QString rootPath = QDir::cleanPath(QStorageInfo(clean).rootPath());
    if (clean == QDir::rootPath() || clean.compare(rootPath, Qt::CaseInsensitive) == 0
        || QDir(clean).isRoot())
        return tr("Deleting everything on a drive is not allowed.");
    const QString home = QDir::cleanPath(QDir::homePath());
    if (clean.compare(home, Qt::CaseInsensitive) == 0
        || home.startsWith(clean + QLatin1Char('/'), Qt::CaseInsensitive))
        return tr("Deleting everything in the home folder or a folder above it is not allowed.");
    return {};
}

bool removeTree(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymbolicLink())
        return true;

    if (isLink(info) || !info.isDir()) {
        if (QFile::remove(path))
            return true;
        // Windows sets a read-only attribute that blocks deleting the file.
        makeWritable(path, false);
        if (QFile::remove(path) || QDir().rmdir(path))
            return true;
        if (error)
            *error = QFile(path).errorString();
        return false;
    }

    makeWritable(path, true);
    bool ok = true;
    const QFileInfoList entries = QDir(path).entryInfoList(AllEntries, QDir::NoSort);
    for (const QFileInfo &e : entries) {
        if (!removeTree(e.filePath(), error))
            ok = false;
    }
    if (ok && !QDir().rmdir(path)) {
        if (error)
            *error = tr("The folder could not be removed.");
        return false;
    }
    return ok;
}

CleanResult clean(const QList<ScanItem> &items, const CancelCheck &canceled)
{
    CleanResult result;
    for (const ScanItem &item : items) {
        if (canceled && canceled())
            break;
        QString error;
        if (removeTree(item.path, &error)) {
            result.freedBytes += item.bytes;
        } else {
            result.freedBytes += qMax<qint64>(0, item.bytes - treeSize(item.path));
            result.failures.append(
                QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(item.path), error));
        }
    }
    return result;
}

} // namespace Scanner
