#pragma once

#include <QList>
#include <QString>

// A folder of the operating system whose contents are safe to delete: a trash, or a cache or
// set of reports that the system or a driver rebuilds when it needs them. Offered by
// SystemFoldersDialog, as Whole folders.
//
// Only folders that nothing depends on are listed. Broad cache folders such as
// ~/Library/Caches and ~/.cache are left out on purpose: they hold the caches of browsers and
// other running programs.
struct SystemFolder
{
    QString name;
    QString path;  // with '/' separators
    QString note;  // what the folder holds, and anything to know before emptying it
};

namespace SystemFolders {

// The folders that exist on this computer, in a fixed order.
QList<SystemFolder> find();

// Whether `path` is inside a trash: a Recycle Bin on Windows, the user's Trash or a volume's
// .Trashes on macOS, or a freedesktop.org trash. Cleaning deletes such items for good instead
// of moving them to the trash again. Only the path is examined; it need not exist.
bool isInTrash(const QString &path);

} // namespace SystemFolders
