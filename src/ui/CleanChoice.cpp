#include "CleanChoice.h"

#include "Format.h"
#include "clean/SystemFolders.h"

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPushButton>

namespace {

QString tr(const char *text, int n = -1)
{
    return QCoreApplication::translate("CleanChoice", text, nullptr, n);
}

int s_shortcutDepth = 0;  // shortcuts of actions with ignoreShiftInShortcuts() now running

// Counts the shortcuts that run, so that method() can ignore the Shift they hold.
class ShortcutFilter : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Shortcut)
            return false;
        ++s_shortcutDepth;
        watched->event(event);
        --s_shortcutDepth;
        return true;
    }
};

} // namespace

namespace CleanChoice {

QString trashName()
{
#ifdef Q_OS_WIN
    return tr("Recycle Bin");
#else
    return tr("Trash");
#endif
}

CleanMethod method(const Settings &settings)
{
    if (settings.deleteDirectly)
        return CleanMethod::Delete;
    const bool shift = s_shortcutDepth == 0
                       && QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
    return shift ? CleanMethod::Delete : CleanMethod::Trash;
}

CleanMethod forFolder(CleanMethod method, const QString &path)
{
    // What cleaning finds is inside the folder.
    if (method == CleanMethod::Trash
        && SystemFolders::isInTrash(path + QStringLiteral("/item")))
        return CleanMethod::Delete;
    return method;
}

void ignoreShiftInShortcuts(QAction *action)
{
    action->installEventFilter(new ShortcutFilter(action));
}

QPushButton *addConfirmButton(QMessageBox &box, bool deletes)
{
    return deletes ? box.addButton(tr("Delete"), QMessageBox::DestructiveRole)
                   : box.addButton(tr("Move to %1").arg(trashName()), QMessageBox::AcceptRole);
}

QString doneMessage(const CleanResult &result, const QString &name)
{
    if (result.trashedBytes > 0 && result.freedBytes > 0)
        return tr("Moved %1 in %2 to the %3 and deleted %4.")
            .arg(Format::bytes(result.trashedBytes), name, trashName(),
                 Format::bytes(result.freedBytes));
    if (result.trashedBytes > 0)
        return tr("Moved %1 in %2 to the %3.")
            .arg(Format::bytes(result.trashedBytes), name, trashName());
    return tr("Freed %1 in %2.").arg(Format::bytes(result.freedBytes), name);
}

QString failureText(const CleanResult &result)
{
    const int n = static_cast<int>(result.failures.size());
    return result.trashFailed ? tr("%n item(s) could not be cleaned.", n)
                              : tr("%n item(s) could not be deleted.", n);
}

QString failureHint(const CleanResult &result)
{
    if (!result.trashFailed)
        return {};
    return tr("Some items could not be moved to the %1 and were left in place. Hold Shift "
              "while you click Clean to delete them directly.")
        .arg(trashName());
}

} // namespace CleanChoice
