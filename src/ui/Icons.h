#pragma once

#include <QIcon>
#include <QImage>

namespace Icons {

// A bin with curly braces on its front and a sparkle above it, drawn in code. The braces keep it
// from looking like the system's trash. The executable's .ico and .icns files are generated
// from the same drawing by tools/make_icons.
QIcon appIcon();
// The icon in the tray or menu bar. Without a warning it is the same shape as a monochrome
// template on macOS. With one it carries a yellow badge, in colour on every platform.
QIcon trayIcon(bool warning = false);
// The icon at one size; tools/make_icons uses it to write the .ico and .icns files.
QImage render(int size, bool mono = false, bool warning = false);
// A toolbar and menu icon. Elsewhere this is QIcon::fromTheme. On macOS the icon is drawn in
// code: Qt 6.8 draws theme icons from SF Symbols, and AppKit aborts the app while it does so on
// macOS 27.
QIcon toolIcon(QIcon::ThemeIcon icon);
// The toolbar's sort buttons for the group list and the folder list, drawn in code everywhere:
// the themes have no sort icons.
QIcon sortGroupsIcon();
QIcon sortFoldersIcon();

} // namespace Icons
