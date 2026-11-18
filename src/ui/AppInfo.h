#pragma once

#include <QString>

namespace AppInfo {

// The name people see: window titles, the tray, dialogs.
inline QString displayName()
{
    return QStringLiteral("Dev Asset Cleaner");
}

inline QString homepage()
{
    return QStringLiteral("https://github.com/rrgmc/dev-asset-cleaner");
}

} // namespace AppInfo
