#include "Folder.h"

#include <QJsonArray>

QJsonObject Folder::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("path"), path},
        {QStringLiteral("types"), QJsonArray::fromStringList(typeIds)},
        // JSON numbers are doubles, exact for whole numbers up to 2^53 bytes.
        {QStringLiteral("warnBytes"), static_cast<double>(warnBytes)},
        {QStringLiteral("excludes"), QJsonArray::fromStringList(excludes)},
    };
}

Folder Folder::fromJson(const QJsonObject &obj)
{
    Folder f;
    f.id = QUuid::fromString(obj.value(QStringLiteral("id")).toString());
    if (f.id.isNull())
        f.id = QUuid::createUuid();
    f.name = obj.value(QStringLiteral("name")).toString();
    f.path = obj.value(QStringLiteral("path")).toString();
    for (const QJsonValue &v : obj.value(QStringLiteral("types")).toArray()) {
        if (!v.toString().isEmpty())
            f.typeIds.append(v.toString());
    }
    f.warnBytes = static_cast<qint64>(obj.value(QStringLiteral("warnBytes")).toDouble(0));
    for (const QJsonValue &v : obj.value(QStringLiteral("excludes")).toArray()) {
        if (!v.toString().isEmpty())
            f.excludes.append(v.toString());
    }
    return f;
}
