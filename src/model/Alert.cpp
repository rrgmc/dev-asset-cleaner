#include "Alert.h"

#include <QCoreApplication>
#include <QJsonArray>

Severity Alert::levelFor(qint64 bytes, Direction direction) const
{
    Severity level = Severity::None;
    for (int i = 0; i < LevelCount; ++i) {
        const qint64 limit = limits[i];
        if (limit <= 0)
            continue;
        const bool passed = direction == Direction::Above ? bytes > limit : bytes < limit;
        if (passed)
            level = static_cast<Severity>(i + 1);
    }
    return level;
}

QString Alert::orderProblem(Direction direction) const
{
    qint64 previous = 0;
    int previousLevel = 0;
    for (int i = 0; i < LevelCount; ++i) {
        const qint64 limit = limits[i];
        if (limit <= 0)
            continue;
        if (previousLevel > 0) {
            const bool ordered = direction == Direction::Above ? limit > previous : limit < previous;
            if (!ordered) {
                const char *text = direction == Direction::Above
                                       ? QT_TRANSLATE_NOOP("Alert", "%1 must be more than %2.")
                                       : QT_TRANSLATE_NOOP("Alert", "%1 must be less than %2.");
                return QCoreApplication::translate("Alert", text)
                    .arg(severityName(static_cast<Severity>(i + 1)),
                         severityName(static_cast<Severity>(previousLevel)));
            }
        }
        previous = limit;
        previousLevel = i + 1;
    }
    return {};
}

QJsonObject Alert::toJson() const
{
    QJsonArray array;
    for (qint64 limit : limits)
        array.append(static_cast<double>(limit));
    return {
        {QStringLiteral("limits"), array},
        {QStringLiteral("notifyFrom"), static_cast<int>(notifyFrom)},
    };
}

Alert Alert::fromJson(const QJsonObject &obj, const Alert &defaults)
{
    Alert a = defaults;
    const QJsonArray array = obj.value(QStringLiteral("limits")).toArray();
    for (int i = 0; i < LevelCount && i < array.size(); ++i)
        a.limits[i] = qMax<qint64>(0, static_cast<qint64>(array.at(i).toDouble(0)));
    const int notify =
        obj.value(QStringLiteral("notifyFrom")).toInt(static_cast<int>(defaults.notifyFrom));
    a.notifyFrom = static_cast<Severity>(qBound(0, notify, LevelCount));
    return a;
}

QString severityName(Severity severity)
{
    switch (severity) {
    case Severity::None:
        return QCoreApplication::translate("Alert", "None");
    case Severity::Low:
        return QCoreApplication::translate("Alert", "Low");
    case Severity::Medium:
        return QCoreApplication::translate("Alert", "Medium");
    case Severity::High:
        return QCoreApplication::translate("Alert", "High");
    }
    return {};
}
