#pragma once

#include "clean/Scanner.h"
#include "model/Store.h"

#include <QString>

class QAction;
class QMessageBox;
class QPushButton;

// Whether cleaning moves to the trash or deletes, and the words for it.
//
// Cleaning moves to the trash unless the settings say to delete directly, or Shift is held
// when the cleaning starts. Items already in a trash are always deleted (Scanner::clean).
namespace CleanChoice {

// The trash's name on this system: "Recycle Bin" on Windows, "Trash" elsewhere.
QString trashName();

// What a cleaning that starts now does. Shift is ignored while a shortcut set up with
// ignoreShiftInShortcuts() runs.
CleanMethod method(const Settings &settings);

// What `method` does to the folder at `path`: Delete when the folder is a trash or inside one.
CleanMethod forFolder(CleanMethod method, const QString &path);

// Makes the shortcuts of `action` choose the method without regard to Shift, which can be
// part of the shortcut itself (Ctrl+Shift+K).
void ignoreShiftInShortcuts(QAction *action);

// Adds the button that confirms cleaning to `box`: Delete, or Move to the trash.
QPushButton *addConfirmButton(QMessageBox &box, bool deletes);

// The status message after cleaning `name`: what was freed and what was moved to the trash.
QString doneMessage(const CleanResult &result, const QString &name);

// The text of the box that lists the failures, and its informative text (empty when there is
// nothing to add).
QString failureText(const CleanResult &result);
QString failureHint(const CleanResult &result);

} // namespace CleanChoice
