#pragma once

#include <functional>

// macOS application events that Qt does not expose.
namespace MacApp {

// Calls `callback` when the user opens dev-asset-cleaner again while it runs: a click on its Dock icon,
// or opening the app in Finder. Unlike Qt's ApplicationActive state, launching does not
// trigger it, so a hidden start stays hidden.
void whenReopened(std::function<void()> callback);

// Keeps modal dialogs from floating over other apps. AppKit raises a dialog run with exec() to
// the modal panel level, and Qt 6.8 lowers only normal and floating windows when the app goes to
// the background. This lowers such dialogs to the normal level while another app is active and
// raises them again when this app comes back.
void lowerModalWindowsWhenInactive();

} // namespace MacApp
