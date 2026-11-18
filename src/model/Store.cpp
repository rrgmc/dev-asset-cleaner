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
constexpr int FileVersion = 1;
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

bool Store::load(QString *error)
{
    m_folders.clear();
    m_customTypes.clear();
    m_settings = Settings{};
    m_isNew = false;

    QFile file(m_filePath);
    if (!file.exists()) {
        m_isNew = true;
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

    for (CleanerType t : cleanerTypesFromJson(root.value(QStringLiteral("types")).toArray())) {
        // A custom type cannot take a built-in id.
        if (std::none_of(BuiltInTypes::all().cbegin(), BuiltInTypes::all().cend(),
                         [&t](const CleanerType &b) { return b.id == t.id; })) {
            t.builtIn = false;
            m_customTypes.append(t);
        }
    }
    for (const QJsonValue &v : root.value(QStringLiteral("folders")).toArray()) {
        if (v.isObject())
            m_folders.append(Folder::fromJson(v.toObject()));
    }
    return true;
}

bool Store::save(QString *error)
{
    m_saveTimer.stop();

    QJsonArray folders;
    for (const Folder &f : m_folders)
        folders.append(f.toJson());

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
        {QStringLiteral("folders"), folders},
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
    if (folder.id.isNull() || rowOf(folder.id) >= 0)
        folder.id = QUuid::createUuid();
    m_folders.append(folder);
    touch();
    return folder.id;
}

void Store::updateFolder(const Folder &folder)
{
    const int row = rowOf(folder.id);
    if (row < 0 || m_folders.at(row) == folder)
        return;
    m_folders[row] = folder;
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
    const int to = std::clamp(row + delta, 0, static_cast<int>(m_folders.size()) - 1);
    if (to == row)
        return;
    m_folders.move(row, to);
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
    for (const QString &id : folder.typeIds) {
        if (const CleanerType *t = type(id))
            result.append(*t);
    }
    if (result.isEmpty())
        result.append(*type(BuiltInTypes::contentsId()));
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
