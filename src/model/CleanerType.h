#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// One thing to look for below a folder: subfolders whose name matches `pattern`, in a folder
// that holds a file matching one of `markers`. With no markers, any folder of that name matches.
// Both use wildcards (`*`, `?`, `[...]`), compared ignoring case.
//
// For example, Rust's build output is `target` next to `Cargo.toml`.
struct CleanRule
{
    QString pattern;
    QStringList markers;

    QJsonObject toJson() const;
    static CleanRule fromJson(const QJsonObject &obj);

    friend bool operator==(const CleanRule &, const CleanRule &) = default;
};

// What cleaning a Matching folder deletes: the subfolders, at any depth, that match one of the
// rules. Deleting everything in a folder is not a type but a kind of folder (Folder::Kind).
struct CleanerType
{
    QString id;  // stable; built-in ids are fixed, custom ones are UUIDs
    QString name;
    QString description;
    QList<CleanRule> rules;
    bool builtIn = false;  // not stored; built-in types come from the code

    QJsonObject toJson() const;
    static CleanerType fromJson(const QJsonObject &obj);

    friend bool operator==(const CleanerType &, const CleanerType &) = default;
};

QJsonArray cleanerTypesToJson(const QList<CleanerType> &types);
// Skips types without an id, and types stored with the old mode "contents", which deleted
// everything in a folder (see Store::load).
QList<CleanerType> cleanerTypesFromJson(const QJsonArray &array);

namespace BuiltInTypes {

// The types that ship with the app.
const QList<CleanerType> &all();

} // namespace BuiltInTypes
