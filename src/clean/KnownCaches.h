#pragma once

#include <QList>
#include <QString>

// Caches of programming tools that are safe to empty: the tools download or rebuild what they
// need again. Offered in the Add menu, with the "Folder contents" type.
struct KnownCache
{
    QString name;
    QString path;  // with '/' separators
};

namespace KnownCaches {

// The caches that exist on this computer, in a fixed order.
QList<KnownCache> find();

} // namespace KnownCaches
