// Writes docs/screenshot.png (Windows) or docs/screenshot-macos.png (macOS), the main window with
// made-up folders and sizes, for the README. Run it after changing the look of the window and
// commit the output:
//
//   cmake --build --preset windows --target make_screenshot
//   build/windows/bin/Release/make_screenshot docs/screenshot.png
//
// On macOS it captures the window with its title bar through screencapture, so the terminal
// needs the Screen Recording permission. Without it, it saves the window's contents only, and
// says so.
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
#include <CoreGraphics/CoreGraphics.h>
#include <QProcess>
#include <unistd.h>
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
        {QStringLiteral("Xcode derived data"), QStringLiteral("Library/Developer/Xcode/DerivedData"),
         {}, 14'700 * MB, 14'700 * MB, 0, 1},
        {QStringLiteral("npm cache"), QStringLiteral(".npm/_cacache"), {}, 2'100 * MB, 2'100 * MB,
         0, 1},
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
    monitor.setVolumesForDisplay(
        {Volume{QStringLiteral("/"), QStringLiteral("Macintosh HD"), 41 * GB, 494 * GB, false}});
}

#ifdef Q_OS_MACOS
// The number of this process's normal window, as screencapture -l wants it, or 0.
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

// Saves the window to out. On macOS, grab() leaves out the title bar and toolbar background, so
// the window server takes the picture instead; -o leaves out the shadow.
bool capture(QWidget &window, const QString &out)
{
#ifdef Q_OS_MACOS
    const int number = windowNumber();
    if (number != 0
        && QProcess::execute(QStringLiteral("screencapture"),
                             {QStringLiteral("-x"), QStringLiteral("-o"),
                              QStringLiteral("-l%1").arg(number), out})
               == 0)
        return true;
    std::fprintf(stderr, "screencapture failed (Screen Recording permission?); saving the "
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
    window.resize(900, 470);
    fill(store, monitor);
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
