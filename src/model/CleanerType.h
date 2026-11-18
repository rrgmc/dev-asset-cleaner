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

// What cleaning a folder deletes.
//
// - Contents: everything inside the folder, but not the folder itself. For caches such as the
//   Go module cache or a downloads folder.
// - Rules: only the subfolders, at any depth, that match one of the rules. For a folder of
//   projects, where only build outputs and dependency folders should go.
struct CleanerType
{
    enum class Mode { Contents, Rules };

    QString id;  // stable; built-in ids are fixed, custom ones are UUIDs
    QString name;
    QString description;
    Mode mode = Mode::Rules;
    QList<CleanRule> rules;
    bool builtIn = false;  // not stored; built-in types come from the code

    QJsonObject toJson() const;
    static CleanerType fromJson(const QJsonObject &obj);

    friend bool operator==(const CleanerType &, const CleanerType &) = default;
};

QJsonArray cleanerTypesToJson(const QList<CleanerType> &types);
QList<CleanerType> cleanerTypesFromJson(const QJsonArray &array);

namespace BuiltInTypes {

// The id of the "Folder contents" type, the default for new folders.
QString contentsId();

// The types that ship with the app, "Folder contents" first.
const QList<CleanerType> &all();

} // namespace BuiltInTypes
