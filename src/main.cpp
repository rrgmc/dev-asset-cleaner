#include "clean/Monitor.h"
#include "model/Store.h"
#include "platform/SingleInstance.h"
#include "ui/AppInfo.h"
#include "ui/Icons.h"
#include "ui/LoginItem.h"
#ifdef Q_OS_MACOS
#include "ui/MacApp.h"
#endif
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>

namespace {

// Loads the store. A file that cannot be parsed is renamed aside so it is never overwritten,
// and the app starts empty.
void loadStore(Store &store)
{
    QString error;
    if (store.load(&error))
        return;
    const QString backup = store.filePath() + QStringLiteral(".broken-")
                           + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QFile::rename(store.filePath(), backup);
    QMessageBox::warning(nullptr, AppInfo::displayName(),
                         QCoreApplication::translate("main",
                                                     "%1 could not be read (%2). It was renamed to "
                                                     "%3 and the app starts with an empty list.")
                             .arg(QDir::toNativeSeparators(store.filePath()), error,
                                  QDir::toNativeSeparators(backup)));
    store.load();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("dev-asset-cleaner"));
    QApplication::setApplicationDisplayName(AppInfo::displayName());
    QApplication::setOrganizationName(QStringLiteral("dev-asset-cleaner"));
    QApplication::setApplicationVersion(QStringLiteral(DAC_VERSION));
    QApplication::setWindowIcon(Icons::appIcon());
    QApplication::setQuitOnLastWindowClosed(false);

    SingleInstance instance;
    if (!instance.start())
        return 0;

    // DAC_DATA_FILE points at another data file, for testing.
    const QString dataFile = qEnvironmentVariable("DAC_DATA_FILE");
    Store store(dataFile.isEmpty() ? Store::defaultFilePath() : dataFile);
    loadStore(store);
    if (store.isNew())
        store.save();
    QObject::connect(&app, &QApplication::aboutToQuit, &store, &Store::flush);

    Monitor monitor(&store);
    // Stop scans and deletions before the store and the window go away.
    QObject::connect(&app, &QApplication::aboutToQuit, &monitor, &Monitor::cancelAll);

    MainWindow window(&store, &monitor);
    QObject::connect(&instance, &SingleInstance::showRequested, &window, &MainWindow::showAndFocus);

#ifdef Q_OS_MACOS
    // Clicking the Dock icon while the window is hidden brings it back.
    MacApp::whenReopened([&window] {
        if (!window.isVisible())
            window.showAndFocus();
    });
    MacApp::attachModalDialogs();
#endif

    // Starting at sign-in keeps the app in the tray until it is called. Windows passes
    // --hidden; macOS passes no arguments, so LoginItem reads it from the launch event.
    const bool hidden = app.arguments().contains(QStringLiteral("--hidden"));
    LoginItem::whenLaunched([&window, hidden](bool atSignIn) {
        if (!hidden && !atSignIn)
            window.showAndFocus();
    });
    return app.exec();
}
