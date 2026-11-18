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

bool isExcluded(const QString &name, const QStringList &excludes)
{
    return excludes.contains(name, Qt::CaseInsensitive);
}

struct CompiledRule
{
    int type = 0;  // index into the types the rules came from
    QRegularExpression pattern;
    QList<QRegularExpression> markers;
};

QList<CompiledRule> compile(const QList<CleanerType> &types)
{
    QList<CompiledRule> rules;
    for (int i = 0; i < types.size(); ++i) {
        for (const CleanRule &r : types.at(i).rules) {
            CompiledRule c{i, wildcard(r.pattern), {}};
            for (const QString &m : r.markers)
                c.markers.append(wildcard(m));
            rules.append(c);
        }
    }
    return rules;
}

// Walks a folder tree and reports every folder that a rule matches. A matched folder is sized
// as a whole and not searched further.
class Walker
{
public:
    // Called with the folder, its size and the indexes of the types whose rules matched it.
    using OnMatch = std::function<void(const QString &path, qint64 bytes, const QList<int> &types)>;

    Walker(QList<CompiledRule> rules, const CancelCheck &canceled, OnMatch onMatch,
           QStringList excludes = {})
        : m_rules(std::move(rules))
        , m_canceled(canceled)
        , m_onMatch(std::move(onMatch))
        , m_excludes(std::move(excludes))
    {
    }

    // Returns the size of everything below dirPath. On the first level, the excluded entries
    // are sized but never matched or searched.
    qint64 walk(const QString &dirPath, bool top = true)
    {
        if (m_canceled && m_canceled())
            return 0;
        const QFileInfoList entries = QDir(dirPath).entryInfoList(AllEntries, QDir::NoSort);
        QStringList names;
        names.reserve(entries.size());
        for (const QFileInfo &e : entries)
            names.append(e.fileName());

        qint64 total = 0;
        for (const QFileInfo &e : entries) {
            if (isLink(e))
                continue;
            if (!e.isDir()) {
                total += e.size();
                continue;
            }
            if (top && isExcluded(e.fileName(), m_excludes)) {
                total += Scanner::treeSize(e.filePath(), m_canceled);
                continue;
            }
            const QList<int> types = matches(e.fileName(), names);
            if (!types.isEmpty()) {
                const qint64 bytes = Scanner::treeSize(e.filePath(), m_canceled);
                m_onMatch(e.filePath(), bytes, types);
                total += bytes;
            } else {
                total += walk(e.filePath(), false);
            }
        }
        return total;
    }

private:
    QList<int> matches(const QString &name, const QStringList &siblings) const
    {
        QList<int> types;
        for (const CompiledRule &rule : m_rules) {
            if (types.contains(rule.type) || !anchoredMatch(rule.pattern, name))
                continue;
            bool found = rule.markers.isEmpty();
            for (qsizetype m = 0; m < rule.markers.size() && !found; ++m) {
                for (const QString &sibling : siblings) {
                    if (anchoredMatch(rule.markers.at(m), sibling)) {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                types.append(rule.type);
        }
        return types;
    }

    QList<CompiledRule> m_rules;
    const CancelCheck &m_canceled;
    OnMatch m_onMatch;
    QStringList m_excludes;
};

// Checks that the folder can be read, lets `fill` find the items, and sorts them.
ScanResult runScan(const QString &path, const CancelCheck &canceled,
                   const std::function<void(ScanResult &)> &fill)
{
    ScanResult result;
    const QFileInfo root(path);
    if (!root.exists() || !root.isDir())
        result.error = tr("The folder does not exist.");
    else if (!root.isReadable())
        result.error = tr("The folder cannot be read.");
    else
        fill(result);
    std::sort(result.items.begin(), result.items.end(),
              [](const ScanItem &a, const ScanItem &b) { return a.bytes > b.bytes; });
    result.valid = !(canceled && canceled());
    result.finishedAt = QDateTime::currentDateTime();
    return result;
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

ScanResult scan(const QString &path, const QList<CleanerType> &types, const CancelCheck &canceled,
                const QStringList &excludes)
{
    return runScan(path, canceled, [&](ScanResult &result) {
        Walker walker(compile(types), canceled,
                      [&result](const QString &item, qint64 bytes, const QList<int> &) {
                          result.items.append(ScanItem{item, bytes});
                          result.cleanableBytes += bytes;
                      },
                      excludes);
        result.totalBytes = walker.walk(path);
    });
}

ScanResult scanWhole(const QString &path, const CancelCheck &canceled, const QStringList &excludes)
{
    return runScan(path, canceled, [&](ScanResult &result) {
        const QFileInfoList entries = QDir(path).entryInfoList(AllEntries, QDir::NoSort);
        for (const QFileInfo &e : entries) {
            if (canceled && canceled())
                break;
            const qint64 bytes = isLink(e) ? 0 : e.isDir() ? treeSize(e.filePath(), canceled)
                                                           : e.size();
            result.totalBytes += bytes;
            if (isExcluded(e.fileName(), excludes))
                continue;
            result.items.append(ScanItem{e.filePath(), bytes});
            result.cleanableBytes += bytes;
        }
    });
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

DetectResult detect(const QString &path, const QList<CleanerType> &types,
                    const CancelCheck &canceled, const QStringList &excludes)
{
    DetectResult result;
    const QFileInfo root(path);
    if (!root.exists() || !root.isDir()) {
        result.error = tr("The folder does not exist.");
        result.valid = true;
        return result;
    }
    QList<CleanerType> candidates;
    for (const CleanerType &t : types) {
        if (!t.rules.isEmpty())
            candidates.append(t);
    }
    QList<Detection> found(candidates.size());
    for (qsizetype i = 0; i < candidates.size(); ++i)
        found[i].typeId = candidates.at(i).id;

    Walker walker(compile(candidates), canceled,
                  [&found, &result](const QString &item, qint64 bytes, const QList<int> &matched) {
                      for (int t : matched) {
                          found[t].items.append(ScanItem{item, bytes});
                          found[t].bytes += bytes;
                      }
                      result.cleanableBytes += bytes;
                  },
                  excludes);
    result.totalBytes = walker.walk(path);

    for (Detection &d : found) {
        if (d.items.isEmpty())
            continue;
        std::sort(d.items.begin(), d.items.end(),
                  [](const ScanItem &a, const ScanItem &b) { return a.bytes > b.bytes; });
        result.detections.append(d);
    }
    std::sort(result.detections.begin(), result.detections.end(),
              [](const Detection &a, const Detection &b) { return a.bytes > b.bytes; });
    result.valid = !(canceled && canceled());
    return result;
}

QString unsafeWholeReason(const QString &path)
{
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
