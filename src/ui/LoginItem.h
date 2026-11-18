#pragma once

#include <QString>

#include <functional>

// Starts dev-asset-cleaner when the user signs in: the per-user Run key on Windows, SMAppService.mainApp
// on macOS 13 and later. The state lives in the OS, so changes made in System Settings or by
// the Windows installer show up here.
namespace LoginItem {

// False on other platforms and before macOS 13.
bool isSupported();

bool isEnabled();

// On failure returns false and describes the problem in `error`.
bool setEnabled(bool enabled, QString *error);

// True when macOS registered dev-asset-cleaner but the user must still allow it in System Settings.
bool needsApproval();

// Opens System Settings > General > Login Items. Does nothing outside macOS.
void openSystemSettings();

// Calls `callback` once dev-asset-cleaner has finished launching, with whether the OS started it at
// sign-in. On macOS the answer comes from the launch Apple event, after QApplication::exec
// starts; elsewhere `callback` runs at once with false, and dev-asset-cleaner relies on --hidden.
void whenLaunched(std::function<void(bool atSignIn)> callback);

} // namespace LoginItem
