#pragma once

#include <QJsonObject>
#include <QString>

#include <array>

// How serious a warning is. Each level has its own colour in the UI (Icons::severityColor).
enum class Severity { None = 0, Low = 1, Medium = 2, High = 3 };

// The three limits of one warning, low to high, and the level from which it sends a
// notification.
//
// For the space to clean, a level starts when more than its limit can be cleaned, so the limits
// grow from Low to High. For free space, a level starts when less than its limit is free, so the
// limits shrink from Low to High. A limit of 0 turns its level off.
struct Alert
{
    enum class Direction { Above, Below };

    static constexpr int LevelCount = 3;

    std::array<qint64, LevelCount> limits{};
    Severity notifyFrom = Severity::Low;  // None: never notify

    // The highest level whose limit the value passes.
    Severity levelFor(qint64 bytes, Direction direction) const;

    // Empty when each level that is on starts beyond the one before it; otherwise why not.
    QString orderProblem(Direction direction) const;

    QJsonObject toJson() const;
    static Alert fromJson(const QJsonObject &obj, const Alert &defaults);

    friend bool operator==(const Alert &, const Alert &) = default;
};

QString severityName(Severity severity);
