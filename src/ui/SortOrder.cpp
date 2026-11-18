#include "SortOrder.h"

#include <QCollator>
#include <QCoreApplication>

#include <algorithm>

namespace Sorting {

QList<SortOrder> all()
{
    return {SortOrder::Manual, SortOrder::Name, SortOrder::Cleanable};
}

QString id(SortOrder order)
{
    switch (order) {
    case SortOrder::Name:
        return QStringLiteral("name");
    case SortOrder::Cleanable:
        return QStringLiteral("cleanable");
    case SortOrder::Manual:
        break;
    }
    return QStringLiteral("manual");
}

SortOrder fromId(const QString &id)
{
    for (SortOrder order : all()) {
        if (Sorting::id(order) == id)
            return order;
    }
    return SortOrder::Manual;
}

QString label(SortOrder order)
{
    switch (order) {
    case SortOrder::Name:
        return QCoreApplication::translate("Sorting", "&Name");
    case SortOrder::Cleanable:
        return QCoreApplication::translate("Sorting", "&Space to clean");
    case SortOrder::Manual:
        break;
    }
    return QCoreApplication::translate("Sorting", "&Manual order");
}

QList<QUuid> sorted(QList<QUuid> ids, SortOrder order,
                    const std::function<QString(const QUuid &)> &name,
                    const std::function<qint64(const QUuid &)> &cleanable)
{
    if (order == SortOrder::Manual || ids.size() < 2)
        return ids;

    // Read each key once: the space to clean comes from the monitor, behind a lock.
    struct Entry {
        QUuid id;
        QString name;
        qint64 bytes = 0;
    };
    QList<Entry> entries;
    entries.reserve(ids.size());
    for (const QUuid &id : std::as_const(ids)) {
        Entry e{id};
        if (order == SortOrder::Name)
            e.name = name(id);
        else
            e.bytes = cleanable(id);
        entries.append(e);
    }

    if (order == SortOrder::Name) {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        std::stable_sort(entries.begin(), entries.end(), [&](const Entry &a, const Entry &b) {
            return collator.compare(a.name, b.name) < 0;
        });
    } else {
        std::stable_sort(entries.begin(), entries.end(),
                         [](const Entry &a, const Entry &b) { return a.bytes > b.bytes; });
    }

    for (qsizetype i = 0; i < entries.size(); ++i)
        ids[i] = entries.at(i).id;
    return ids;
}

} // namespace Sorting
