#pragma once

#include "model/CleanerType.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// Something that cleaning would delete: a file or a folder with everything below it.
struct ScanItem
{
    QString path;
    qint64 bytes = 0;
};

struct ScanResult
{
    bool valid = false;        // false until a scan has finished
    qint64 totalBytes = 0;     // everything below the folder
    qint64 cleanableBytes = 0; // what cleaning would delete
    QList<ScanItem> items;     // largest first
    QString error;             // the folder is missing or cannot be read
    QDateTime finishedAt;
};

struct CleanResult
{
    qint64 freedBytes = 0;
    QStringList failures;  // one line per path that could not be deleted
};

// What one cleaner type would delete in a folder.
struct Detection
{
    QString typeId;
    qint64 bytes = 0;
    QList<ScanItem> items;  // largest first
};

// The cleaner types found in a folder.
struct DetectResult
{
    bool valid = false;         // false when canceled
    qint64 totalBytes = 0;      // everything below the folder
    qint64 cleanableBytes = 0;  // what all detected types together would delete
    QList<Detection> detections;  // largest first; only types that found something
    QString error;
};

// Returns true to stop the work early.
using CancelCheck = std::function<bool()>;

namespace Scanner {

// Walks `path` and finds what `types` would delete. Symbolic links are counted as their own
// size and never followed. Sizes are file sizes, not the disk blocks they use.
//
// `excludes` names entries directly inside `path` (first-level subfolders) that are never
// deleted or searched; they still count toward the folder's total size. Names compare without
// regard to case.
ScanResult scan(const QString &path, const QList<CleanerType> &types,
                const CancelCheck &canceled = {}, const QStringList &excludes = {});

// Finds which of `types` apply to `path`: walks it once with the rules of every rule-based type
// and reports, per type, what it would delete. A folder that several types match counts for
// each of them. "Folder contents" types are never detected, since they match any folder and
// would delete everything in it. `excludes` works as in scan().
//
// Today detection covers the rule-based types, which are programming build outputs and
// dependencies. Other kinds of detection can add their findings to DetectResult later.
DetectResult detect(const QString &path, const QList<CleanerType> &types,
                    const CancelCheck &canceled = {}, const QStringList &excludes = {});

// The size of a file, or of a folder and everything below it.
qint64 treeSize(const QString &path, const CancelCheck &canceled = {});

// Why cleaning `path` with `types` is refused, or an empty string. "Folder contents" is refused
// on a drive's root, the home folder and the folders above it.
QString unsafeReason(const QString &path, const QList<CleanerType> &types);

// Deletes the items. Read-only files and folders (such as Go's module cache) are made
// writable first.
CleanResult clean(const QList<ScanItem> &items, const CancelCheck &canceled = {});

// Deletes a file, or a folder and everything below it. Does not follow symbolic links.
bool removeTree(const QString &path, QString *error = nullptr);

} // namespace Scanner
