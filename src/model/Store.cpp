#include "Store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {
// 1: folders at the top level. 2: folders nested in groups. 3: folders have a kind, and
// "Folder contents" is no longer a cleaner type.
constexpr int FileVersion = 3;
constexpr int SaveDelayMs = 300;

double toJsonNumber(qint64 value)
{
    return static_cast<double>(value);
}
} // namespace

Store::Store(QString filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        QString error;
        if (!save(&error))
            emit saveFailed(error);
    });
}

Store::~Store()
{
    flush();
}

QString Store::defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("dev-asset-cleaner.json"));
}

QString Store::defaultGroupName()
{
    return tr("Default");
}

bool Store::load(QString *error)
{
    m_groups.clear();
    m_folders.clear();
    m_customTypes.clear();
    m_settings = Settings{};
    m_isNew = false;

    QFile file(m_filePath);
    if (!file.exists()) {
        m_isNew = true;
        m_groups.append(Group{QUuid::createUuid(), defaultGroupName()});
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = parseError.error != QJsonParseError::NoError
                         ? parseError.errorString()
                         : tr("The file does not contain a JSON object.");
        }
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    const Settings defaults;
    m_settings.closeToTray = settings.value(QStringLiteral("closeToTray")).toBool(defaults.closeToTray);
    m_settings.rescanMinutes =
        settings.value(QStringLiteral("rescanMinutes")).toInt(defaults.rescanMinutes);
    m_settings.freeWarnBytes = static_cast<qint64>(
        settings.value(QStringLiteral("freeWarnBytes")).toDouble(toJsonNumber(defaults.freeWarnBytes)));
    m_settings.totalWarnBytes = static_cast<qint64>(
        settings.value(QStringLiteral("totalWarnBytes")).toDouble(toJsonNumber(defaults.totalWarnBytes)));

    const QJsonArray types = root.value(QStringLiteral("types")).toArray();
    for (CleanerType t : cleanerTypesFromJson(types)) {
        // A custom type cannot take a built-in id.
        if (std::none_of(BuiltInTypes::all().cbegin(), BuiltInTypes::all().cend(),
                         [&t](const CleanerType &b) { return b.id == t.id; })) {
            t.builtIn = false;
            m_customTypes.append(t);
        }
    }
    // Before version 3, deleting everything in a folder was a cleaner type: the built-in
    // "contents", or a custom type with that mode. Such folders, and folders with no types,
    // become Whole folders.
    QStringList contentsTypes{QStringLiteral("contents")};
    for (const QJsonValue &v : types) {
        if (v.toObject().value(QStringLiteral("mode")).toString() == QLatin1String("contents"))
            contentsTypes.append(v.toObject().value(QStringLiteral("id")).toString());
    }
    const auto addFolders = [this, &contentsTypes](const QJsonArray &folders, const QUuid &groupId) {
        for (const QJsonValue &v : folders) {
            if (!v.isObject())
                continue;
            Folder f = Folder::fromJson(v.toObject());
            if (!v.toObject().contains(QStringLiteral("kind"))) {
                const bool whole = f.typeIds.isEmpty()
                                   || std::any_of(f.typeIds.cbegin(), f.typeIds.cend(),
                                                  [&contentsTypes](const QString &id) {
                                                      return contentsTypes.contains(id);
                                                  });
                f.kind = whole ? Folder::Kind::Whole : Folder::Kind::Matching;
                if (whole)
                    f.typeIds.clear();
            }
            if (rowOf(f.id) >= 0)
                f.id = QUuid::createUuid();
            f.groupId = groupId;
            m_folders.append(f);
        }
    };
    if (root.contains(QStringLiteral("groups"))) {
        for (const QJsonValue &v : root.value(QStringLiteral("groups")).toArray()) {
            const QJsonObject obj = v.toObject();
            Group g{QUuid::fromString(obj.value(QStringLiteral("id")).toString()),
                    obj.value(QStringLiteral("name")).toString()};
            if (g.id.isNull() || groupRowOf(g.id) >= 0)
                g.id = QUuid::createUuid();
            m_groups.append(g);
            addFolders(obj.value(QStringLiteral("folders")).toArray(), g.id);
        }
    } else {
        // Version 1 had no groups.
        m_groups.append(Group{QUuid::createUuid(), defaultGroupName()});
        addFolders(root.value(QStringLiteral("folders")).toArray(), m_groups.first().id);
    }
    return true;
}

bool Store::save(QString *error)
{
    m_saveTimer.stop();

    QJsonArray groups;
    for (const Group &g : m_groups) {
        QJsonArray folders;
        for (const Folder &f : m_folders) {
            if (f.groupId == g.id)
                folders.append(f.toJson());
        }
        groups.append(QJsonObject{
            {QStringLiteral("id"), g.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("name"), g.name},
            {QStringLiteral("folders"), folders},
        });
    }

    const QJsonObject root{
        {QStringLiteral("version"), FileVersion},
        {QStringLiteral("settings"),
         QJsonObject{
             {QStringLiteral("closeToTray"), m_settings.closeToTray},
             {QStringLiteral("rescanMinutes"), m_settings.rescanMinutes},
             {QStringLiteral("freeWarnBytes"), toJsonNumber(m_settings.freeWarnBytes)},
             {QStringLiteral("totalWarnBytes"), toJsonNumber(m_settings.totalWarnBytes)},
         }},
        {QStringLiteral("types"), cleanerTypesToJson(m_customTypes)},
        {QStringLiteral("groups"), groups},
    };

    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_isNew = false;
    return true;
}

void Store::flush()
{
    if (m_saveTimer.isActive())
        save();
}

const Group *Store::group(const QUuid &id) const
{
    const int row = groupRowOf(id);
    return row < 0 ? nullptr : &m_groups.at(row);
}

QUuid Store::addGroup(const QString &name)
{
    const Group g{QUuid::createUuid(), name};
    m_groups.append(g);
    touch();
    return g.id;
}

void Store::renameGroup(const QUuid &id, const QString &name)
{
    const int row = groupRowOf(id);
    if (row < 0 || m_groups.at(row).name == name)
        return;
    m_groups[row].name = name;
    touch();
}

void Store::removeGroup(const QUuid &id)
{
    const int row = groupRowOf(id);
    if (row < 0)
        return;
    m_groups.removeAt(row);
    m_folders.removeIf([&id](const Folder &f) { return f.groupId == id; });
    touch();
}

void Store::moveGroupBy(const QUuid &id, int delta)
{
    const int row = groupRowOf(id);
    if (row < 0)
        return;
    const int to = std::clamp(row + delta, 0, static_cast<int>(m_groups.size()) - 1);
    if (to == row)
        return;
    m_groups.move(row, to);
    touch();
}

QList<Folder> Store::foldersIn(const QList<QUuid> &groupIds) const
{
    QList<Folder> result;
    for (const Group &g : m_groups) {
        if (!groupIds.contains(g.id))
            continue;
        for (const Folder &f : m_folders) {
            if (f.groupId == g.id)
                result.append(f);
        }
    }
    return result;
}

const Folder *Store::folder(const QUuid &id) const
{
    const int row = rowOf(id);
    return row < 0 ? nullptr : &m_folders.at(row);
}

const Folder *Store::folderWithPath(const QString &path) const
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    for (const Folder &f : m_folders) {
        if (QDir::cleanPath(QDir::fromNativeSeparators(f.path)).compare(clean, Qt::CaseInsensitive)
            == 0)
            return &f;
    }
    return nullptr;
}

QUuid Store::addFolder(Folder folder)
{
    if (groupRowOf(folder.groupId) < 0)
        return {};
    if (folder.id.isNull() || rowOf(folder.id) >= 0)
        folder.id = QUuid::createUuid();
    m_folders.append(folder);
    touch();
    return folder.id;
}

void Store::updateFolder(const Folder &folder)
{
    const int row = rowOf(folder.id);
    if (row < 0)
        return;
    Folder updated = folder;
    updated.groupId = m_folders.at(row).groupId;
    if (m_folders.at(row) == updated)
        return;
    m_folders[row] = updated;
    touch();
}

void Store::removeFolder(const QUuid &id)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    m_folders.removeAt(row);
    touch();
}

void Store::moveBy(const QUuid &id, int delta)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    // The rows of the group's folders, in order; move to the one `delta` places away.
    QList<int> rows;
    for (int i = 0; i < m_folders.size(); ++i) {
        if (m_folders.at(i).groupId == m_folders.at(row).groupId)
            rows.append(i);
    }
    const int position = static_cast<int>(rows.indexOf(row));
    const int to = std::clamp(position + delta, 0, static_cast<int>(rows.size()) - 1);
    if (to == position)
        return;
    m_folders.move(row, rows.at(to));
    touch();
}

void Store::moveToGroup(const QUuid &id, const QUuid &groupId)
{
    const int row = rowOf(id);
    if (row < 0 || groupRowOf(groupId) < 0 || m_folders.at(row).groupId == groupId)
        return;
    Folder f = m_folders.takeAt(row);
    f.groupId = groupId;
    m_folders.append(f);
    touch();
}

QList<CleanerType> Store::types() const
{
    return BuiltInTypes::all() + m_customTypes;
}

void Store::setCustomTypes(const QList<CleanerType> &types)
{
    if (m_customTypes == types)
        return;
    m_customTypes = types;
    touch();
}

const CleanerType *Store::type(const QString &id) const
{
    for (const CleanerType &t : BuiltInTypes::all()) {
        if (t.id == id)
            return &t;
    }
    for (const CleanerType &t : m_customTypes) {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

QList<CleanerType> Store::typesFor(const Folder &folder) const
{
    QList<CleanerType> result;
    if (folder.kind != Folder::Kind::Matching)
        return result;
    for (const QString &id : folder.typeIds) {
        if (const CleanerType *t = type(id))
            result.append(*t);
    }
    return result;
}

void Store::setSettings(const Settings &settings)
{
    if (m_settings == settings)
        return;
    m_settings = settings;
    scheduleSave();
    emit settingsChanged();
}

int Store::rowOf(const QUuid &id) const
{
    for (int i = 0; i < m_folders.size(); ++i) {
        if (m_folders.at(i).id == id)
            return i;
    }
    return -1;
}

int Store::groupRowOf(const QUuid &id) const
{
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups.at(i).id == id)
            return i;
    }
    return -1;
}

void Store::scheduleSave()
{
    if (m_autoSave)
        m_saveTimer.start();
}

void Store::touch()
{
    scheduleSave();
    emit changed();
}
