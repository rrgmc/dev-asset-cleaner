#pragma once

#include "model/Alert.h"

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>

namespace Icons {

// The colour of a warning level, for badges: yellow, orange and red.
QColor severityColor(Severity severity);
// A darker shade of the same colour that reads as text on a light background.
QColor severityTextColor(Severity severity);

// The tray icon's badge for one warning alone, `size` pixels square, for the settings.
QPixmap badge(Severity severity, bool freeSpace, int size);

// A bin with curly braces on its front and a sparkle above it, drawn in code. The braces keep it
// from looking like the system's trash. The executable's .ico and .icns files are generated
// from the same drawing by tools/make_icons.
QIcon appIcon();
// The icon in the tray or menu bar, without warnings: a monochrome template on macOS, the app
// icon elsewhere.
QIcon trayIcon();
// The warning badges that TrayIcon draws over trayIcon(), each in its level's colour: a circle at
// the bottom left for the space to clean, and a triangle pointing down at the bottom right for
// free space. Null without a warning.
QIcon trayBadges(Severity clean, Severity free);
// The icon at one size; tools/make_icons uses it to write the .ico and .icns files.
QImage render(int size, bool mono = false, Severity clean = Severity::None,
              Severity free = Severity::None);
// A toolbar and menu icon. Elsewhere this is QIcon::fromTheme. On macOS the icon is drawn in
// code: Qt 6.8 draws theme icons from SF Symbols, and AppKit aborts the app while it does so on
// macOS 27.
QIcon toolIcon(QIcon::ThemeIcon icon);
// The toolbar's sort buttons for the group list and the folder list, drawn in code everywhere:
// the themes have no sort icons.
QIcon sortGroupsIcon();
QIcon sortFoldersIcon();

} // namespace Icons
