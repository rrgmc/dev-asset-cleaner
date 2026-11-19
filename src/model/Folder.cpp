#include "Folder.h"

#include <QCoreApplication>
#include <QJsonArray>

QList<Folder::Kind> Folder::kinds()
{
    return {Kind::Matching, Kind::Whole};
}

QString Folder::kindId(Kind kind)
{
    switch (kind) {
    case Kind::Whole:
        return QStringLiteral("whole");
    case Kind::Matching:
        break;
    }
    return QStringLiteral("matching");
}

QString Folder::kindName(Kind kind)
{
    switch (kind) {
    case Kind::Whole:
        return QCoreApplication::translate("Folder", "Whole folder");
    case Kind::Matching:
        break;
    }
    return QCoreApplication::translate("Folder", "Matching folders");
}

QString Folder::kindDescription(Kind kind)
{
    switch (kind) {
    case Kind::Whole:
        return QCoreApplication::translate(
            "Folder", "Deletes everything inside the folder. The folder itself stays. For caches "
                      "and download folders.");
    case Kind::Matching:
        break;
    }
    return QCoreApplication::translate(
        "Folder", "Deletes only the subfolders, at any depth, that the checked types match, such "
                  "as build outputs and dependencies. For a folder of projects.");
}

QJsonObject Folder::toJson() const
{
    return QJsonObject{
        {QStringLiteral("id"), id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("path"), path},
        {QStringLiteral("kind"), kindId(kind)},
        // A Whole folder writes no types, which older versions read as "Folder contents".
        {QStringLiteral("types"),
         QJsonArray::fromStringList(kind == Kind::Whole ? QStringList() : typeIds)},
        // JSON numbers are doubles, exact for whole numbers up to 2^53 bytes.
        {QStringLiteral("warnBytes"), static_cast<double>(warnBytes)},
        {QStringLiteral("excludes"), QJsonArray::fromStringList(excludes)},
        {QStringLiteral("inWarnings"), inWarnings},
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
    // Files without a kind are read by Store::load, which knows the old "contents" types.
    const QString kind = obj.value(QStringLiteral("kind")).toString();
    f.kind = Kind::Matching;
    for (Kind k : kinds()) {
        if (kindId(k) == kind)
            f.kind = k;
    }
    for (const QJsonValue &v : obj.value(QStringLiteral("types")).toArray()) {
        if (!v.toString().isEmpty())
            f.typeIds.append(v.toString());
    }
    f.warnBytes = static_cast<qint64>(obj.value(QStringLiteral("warnBytes")).toDouble(0));
    for (const QJsonValue &v : obj.value(QStringLiteral("excludes")).toArray()) {
        if (!v.toString().isEmpty())
            f.excludes.append(v.toString());
    }
    f.inWarnings = obj.value(QStringLiteral("inWarnings")).toBool(true);
    if (f.kind == Kind::Whole)
        f.typeIds.clear();
    return f;
}
