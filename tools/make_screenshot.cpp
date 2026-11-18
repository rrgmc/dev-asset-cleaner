// Writes docs/screenshot.png (Windows) or docs/screenshot-macos.png (macOS), the main window with
// made-up folders and sizes, for the README. Run it after changing the look of the window and
// commit the output:
//
//   cmake --build --preset windows --target make_screenshot
//   build/windows/bin/Release/make_screenshot docs/screenshot.png
//
// It captures the window with its title bar: through PrintWindow on Windows, and through
// ScreenCaptureKit on macOS, where the terminal needs the Screen Recording permission. Without it,
// it saves the window's contents only, and says so.
//
// The data lives in a temporary file, so the real data is not touched.

#include "clean/Monitor.h"
#include "model/Store.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QLocale>
#include <QTemporaryDir>
#include <QThread>

#ifdef Q_OS_MACOS
#include "WindowCapture_mac.h"

#include <CoreGraphics/CoreGraphics.h>
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <QImage>
#include <dwmapi.h>
#include <windows.h>
#endif

#include <cstdio>

namespace {

#ifdef Q_OS_MACOS
const QString Home = QStringLiteral("/Users/me/");
#else
const QString Home = QStringLiteral("C:/Users/me/");
#endif

constexpr qint64 GB = 1'000'000'000;
constexpr qint64 MB = 1'000'000;

struct Sample
{
    QString name;
    QString path;
    QStringList types;  // none: a Whole folder
    qint64 total;
    qint64 cleanable;
    qint64 warn = 0;
    int group = 0;  // 0: Projects, 1: Tool caches
};

void fill(Store &store, Monitor &monitor)
{
    const QList<Sample> samples = {
        {QStringLiteral("Projects"), QStringLiteral("Projects"), {QStringLiteral("all-languages")},
         84 * GB, 37'400 * MB, 30 * GB},
        {QStringLiteral("Rust experiments"), QStringLiteral("src/rust"), {QStringLiteral("rust")},
         21'300 * MB, 19'800 * MB},
        {QStringLiteral("Go module cache"), QStringLiteral("go/pkg/mod"), {}, 6'200 * MB,
         6'200 * MB, 0, 1},
#ifdef Q_OS_MACOS
        {QStringLiteral("Xcode derived data"), QStringLiteral("Library/Developer/Xcode/DerivedData"),
         {}, 14'700 * MB, 14'700 * MB, 0, 1},
        {QStringLiteral("npm cache"), QStringLiteral(".npm/_cacache"), {}, 2'100 * MB, 2'100 * MB,
         0, 1},
#else
        {QStringLiteral("NuGet packages"), QStringLiteral(".nuget/packages"), {}, 14'700 * MB,
         14'700 * MB, 0, 1},
        {QStringLiteral("npm cache"), QStringLiteral("AppData/Local/npm-cache"), {}, 2'100 * MB,
         2'100 * MB, 0, 1},
#endif
        {QStringLiteral("Work"), QStringLiteral("Work"),
         {QStringLiteral("node"), QStringLiteral("python")}, 12'800 * MB, 0},
    };
    // The new store's "Default" group holds the projects; the caches get a group of their own.
    const QList<QUuid> groups{store.groups().first().id, store.addGroup(QStringLiteral("Tool caches"))};
    store.renameGroup(groups.first(), QStringLiteral("Projects"));
    store.addGroup(QStringLiteral("System"));
    QList<QUuid> ids;
    for (const Sample &s : samples) {
        Folder f;
        f.name = s.name;
        f.path = Home + s.path;
        f.kind = s.types.isEmpty() ? Folder::Kind::Whole : Folder::Kind::Matching;
        f.typeIds = s.types;
        f.warnBytes = s.warn;
        f.groupId = groups.at(s.group);
        ids.append(store.addFolder(f));
    }
    // The real scans of the made-up paths fail quickly; replace their results afterwards.
    while (monitor.isBusy()) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    for (qsizetype i = 0; i < samples.size(); ++i) {
        ScanResult r;
        r.valid = true;
        r.totalBytes = samples.at(i).total;
        r.cleanableBytes = samples.at(i).cleanable;
        r.finishedAt = QDateTime::currentDateTime();
        monitor.setResultForDisplay(ids.at(i), r);
    }
#ifdef Q_OS_MACOS
    monitor.setVolumesForDisplay(
        {Volume{QStringLiteral("/"), QStringLiteral("Macintosh HD"), 41 * GB, 494 * GB}});
#else
    monitor.setVolumesForDisplay(
        {Volume{QStringLiteral("C:/"), QStringLiteral("Windows"), 41 * GB, 494 * GB}});
#endif
}

#ifdef Q_OS_MACOS
// The window server number of this process's normal window, or 0.
int windowNumber()
{
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!list)
        return 0;
    int number = 0;
    for (CFIndex i = 0; i < CFArrayGetCount(list) && number == 0; ++i) {
        const auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(list, i));
        int pid = 0;
        int layer = -1;
        CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowOwnerPID)),
                         kCFNumberIntType, &pid);
        CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer)),
                         kCFNumberIntType, &layer);
        if (pid == getpid() && layer == 0)
            CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowNumber)),
                             kCFNumberIntType, &number);
    }
    CFRelease(list);
    return number;
}
#endif

#ifdef Q_OS_WIN
// The window with its title bar, as the screen shows it, or a null image. PrintWindow draws the
// whole window, including the invisible resize borders, so the picture is cut to the visible
// frame that DWM reports.
QImage windowImage(QWidget &window)
{
    const auto hwnd = reinterpret_cast<HWND>(window.winId());
    RECT outer{};
    RECT visible{};
    if (!GetWindowRect(hwnd, &outer)
        || FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visible,
                                        sizeof(visible))))
        return {};
    const int width = outer.right - outer.left;
    const int height = outer.bottom - outer.top;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ old = SelectObject(dc, bitmap);
    const bool ok = PrintWindow(hwnd, dc, PW_RENDERFULLCONTENT);
    SelectObject(dc, old);
    QImage image = ok ? QImage::fromHBITMAP(bitmap) : QImage();
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    if (image.isNull())
        return {};
    return image
        .copy(visible.left - outer.left, visible.top - outer.top, visible.right - visible.left,
              visible.bottom - visible.top)
        .convertToFormat(QImage::Format_RGB32);
}
#endif

// Saves the window to out. grab() leaves out the title bar (and on macOS the toolbar background),
// so on macOS ScreenCaptureKit takes the picture instead, without the shadow, and on Windows
// PrintWindow does.
bool capture(QWidget &window, const QString &out)
{
#ifdef Q_OS_WIN
    const QImage image = windowImage(window);
    if (!image.isNull())
        return image.save(out);
    std::fprintf(stderr, "PrintWindow failed; saving the window contents without the title bar\n");
#endif
#ifdef Q_OS_MACOS
    const int number = windowNumber();
    if (number != 0 && captureWindowMac(static_cast<unsigned int>(number), out))
        return true;
    std::fprintf(stderr, "window capture failed (Screen Recording permission?); saving the "
                         "window contents without the title bar\n");
#endif
    return window.grab().save(out);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Keep QSettings (window geometry) away from the real application's.
    QApplication::setOrganizationName(QStringLiteral("dev-asset-cleaner-screenshot"));
    QApplication::setWindowIcon(Icons::appIcon());
    // The same number format whatever this computer's language.
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
#ifdef Q_OS_MACOS
    const QString defaultOut = QStringLiteral("docs/screenshot-macos.png");
#else
    const QString defaultOut = QStringLiteral("docs/screenshot.png");
#endif
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultOut;

    QTemporaryDir dir;
    Store store(dir.filePath(QStringLiteral("data.json")));
    store.setAutoSave(false);
    store.load();
    Settings settings = store.settings();
    settings.rescanMinutes = 0;
    store.setSettings(settings);
    Monitor monitor(&store);

    MainWindow window(&store, &monitor);
    fill(store, monitor);
    // Wide enough for the whole toolbar, so that none of its buttons go into the overflow menu,
    // with room left for the filter field to show its placeholder text.
    window.resize(qMax(900, window.sizeHint().width() + 120), 470);
    window.showAndFocus();
    // Let the window lay out and paint before grabbing it.
    for (int i = 0; i < 20; ++i) {
        QApplication::processEvents();
        QThread::msleep(25);
    }

    if (!capture(window, out)) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(out));
        return 1;
    }
    std::printf("wrote %s\n", qPrintable(QDir::toNativeSeparators(out)));
    return 0;
}
