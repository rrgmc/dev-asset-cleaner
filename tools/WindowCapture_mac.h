#pragma once

#include <QString>

// Saves the on-screen window with this window server number, title bar included and shadow left
// out, as a PNG file through ScreenCaptureKit (macOS 14 and later). Needs the Screen Recording
// permission. screencapture -l can no longer capture a single window on macOS 27.
bool captureWindowMac(unsigned int windowNumber, const QString &out);
