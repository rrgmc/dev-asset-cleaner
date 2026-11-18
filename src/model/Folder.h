#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QUuid>

// A monitored folder.
struct Folder
{
    QUuid id;
    QString name;
    QString path;          // with '/' separators
    QStringList typeIds;   // cleaner types; empty means "Folder contents"
    qint64 warnBytes = 0;  // warn when the folder holds more than this; 0 turns it off

    QJsonObject toJson() const;
    static Folder fromJson(const QJsonObject &obj);

    friend bool operator==(const Folder &, const Folder &) = default;
};
