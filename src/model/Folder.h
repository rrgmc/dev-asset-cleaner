#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QUuid>

// A monitored folder.
struct Folder
{
    // What cleaning deletes.
    // - Whole: everything inside the folder, but not the folder itself. For caches such as the
    //   Go module cache or a downloads folder.
    // - Matching: only the subfolders, at any depth, that the folder's cleaner types match. For
    //   a folder of projects, where only build outputs and dependency folders should go.
    // Stored by name (kindId); a name this version does not know loads as Matching, which
    // deletes nothing without types.
    enum class Kind { Whole, Matching };

    static QList<Kind> kinds();  // in the order the folder dialog offers them
    static QString kindId(Kind kind);
    static QString kindName(Kind kind);
    static QString kindDescription(Kind kind);

    QUuid id;
    QString name;
    QString path;          // with '/' separators
    QStringList typeIds;   // cleaner types of a Matching folder; a Whole folder has none
    qint64 warnBytes = 0;  // warn when more than this can be cleaned; 0 turns it off
    QUuid groupId;         // not in the folder's JSON: the file nests folders in their group
    QStringList excludes;  // names of first-level subfolders that cleaning never touches
    Kind kind = Kind::Matching;

    QJsonObject toJson() const;
    static Folder fromJson(const QJsonObject &obj);

    friend bool operator==(const Folder &, const Folder &) = default;
};
