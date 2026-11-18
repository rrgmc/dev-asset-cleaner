#pragma once

#include <QList>
#include <QString>
#include <QUuid>

#include <functional>

// How the group list and the folder list order their rows.
enum class SortOrder {
    Manual,     // the store's order, which Move up and Move down change
    Name,       // by name, ignoring case, with numbers in numeric order
    Cleanable,  // the most space to clean first
};

namespace Sorting {

// Every order, in menu order, with its stored id and its menu label.
QList<SortOrder> all();
QString id(SortOrder order);
SortOrder fromId(const QString &id);  // an unknown id gives Manual
QString label(SortOrder order);

// Returns ids sorted by order. Ties keep the order of ids.
QList<QUuid> sorted(QList<QUuid> ids, SortOrder order,
                    const std::function<QString(const QUuid &)> &name,
                    const std::function<qint64(const QUuid &)> &cleanable);

} // namespace Sorting
