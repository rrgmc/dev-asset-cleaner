#pragma once

#include <QString>
#include <QUuid>

// A named list of monitored folders. Every folder belongs to one group.
struct Group
{
    QUuid id;
    QString name;

    friend bool operator==(const Group &, const Group &) = default;
};
