# Dev Asset Cleaner

A Qt 6 Widgets app (C++20, CMake) for Windows and macOS that watches the space folders use and
deletes build outputs and caches in them. `README.md` describes the app for users, and `BUILDING.md`
describes the build, packaging and releases. The build, packaging, release and platform code come from the sibling project favdir;
keep fixes to those parts in step with it.

## Worktrees

This repository requires git worktrees. Before the first edit, call `EnterWorktree` to make a
feature branch with its own checkout; do not branch in place with `git checkout -b`. Never
work in the main checkout's working tree, which stays on `master`.

- Worktrees go in `.claude/worktrees/` (gitignored).
- Each worktree has its own `build/` folder, so the first build in a new worktree configures
  from scratch.
- Qt is shared only through `QT_ROOT`. Without it, `scripts/setup.ps1` or `scripts/setup.sh`
  downloads Qt again into the worktree's own `.qt/`, so set `QT_ROOT` first.
- `master` is protected on GitHub: nobody, admins included, can push to it directly. Changes
  reach it only through a pull request that passes the `ci` check (see Packaging and releases).
  No approval is needed. Force pushes to `master` and deleting it are refused. Tags are not
  affected.
- To merge, push the branch, open a pull request and merge it on GitHub, deleting the remote
  branch. Then pull `master` in the main checkout, remove the worktree and delete the local
  branch.

## Build and test

- Qt is a prebuilt download, fetched by `scripts/setup.ps1` or `scripts/setup.sh` into the
  folder named by the `QT_ROOT` environment variable (a Qt folder shared between projects), or
  else into `.qt/` (gitignored). The presets give the folder below that root
  (`DAC_QT_SUBDIR`) and `CMakeLists.txt` adds the root. The Qt version is pinned in both
  scripts and in `CMakePresets.json`; change all three together. CMake caches the Qt it found,
  so configure with `--fresh` after changing `QT_ROOT`.
- Presets:
  - `windows`: Visual Studio 2022 generator. Use it for local work.
  - `windows-ninja`: Ninja Multi-Config with whatever MSVC is in the environment. CI and the
    release workflow use it; locally it needs a developer prompt (`vcvars64.bat`).
  - `macos`: Ninja Multi-Config, universal binary.
- `cmake --preset macos && cmake --build --preset macos && ctest --preset macos`.
- `Taskfile.yml` (go-task) wraps setup, build, test, run, install, package, icons and
  screenshot for the current platform's preset. Keep it in step with the commands here.
- Run tests through CTest, not the test executables directly. CTest sets
  `QT_QPA_PLATFORM=offscreen`, `QT_PLUGIN_PATH` and `PATH`. Tests log in TAP (`-o -,tap`)
  because Qt Test's plain-text logger prints nothing to a pipe on Windows.
- Starting the app while another instance runs only tells that instance to show itself
  (`SingleInstance`). To try a build without touching the user's data, set `DAC_DATA_FILE` to a
  scratch file. Never point a test or a trial run at a real folder as a Whole folder: it
  deletes everything in it.

## Layout

- `src/model`: `CleanerType` (rules and the built-in types), `Group`, `Folder`, `Store` (JSON
  persistence, debounced atomic saves). No widgets. A folder's `Folder::Kind` says what
  cleaning deletes: everything in it (`Whole`) or what its types match (`Matching`). The kind
  is chosen when the folder is added (Add > Folder, or Add > Matching folders through
  `DetectDialog`) and never changes afterwards: `FolderDialog` only shows it, and
  `DetectDialog` refuses a Whole folder's path. More kinds may come; each needs an entry in
  `Folder::kinds`, its id, name and description, its scan in `Monitor`'s `folderScan`, and a
  way to add it. An unknown kind id loads as `Matching`, which deletes
  nothing without types. Every folder belongs to a group
  (`Folder::groupId`); the file nests folders in their groups (version 2), and a version 1
  file's top-level folders load into a "Default" group. Version 3 added the kind; before it,
  "Folder contents" was a cleaner type (`contents`, or a custom type with mode `contents`),
  and `Store::load` turns folders that used it, or had no types, into Whole folders. A new file starts with an empty
  "Default" group, which is not special: deleting every group is kept.
- `src/clean`: `Scanner` (walks a folder, finds what its types match, deletes it; `detect`
  reports per type what each rule-based type would delete, for `DetectDialog`;
  `scanSubfolder` finds what `scan` would find in one first-level subfolder),
  `Monitor` (runs scans and cleans on a thread pool, keeps results, free space and warnings),
  `KnownCaches` (tool caches offered in the Add menu), `SystemFolders` (the OS's trash and
  cache folders that are safe to empty, offered by `SystemFoldersDialog`; keep browser caches
  and broad folders such as `~/Library/Caches` out of it).
- `src/ui`: widgets. `MainWindow` holds a `GroupListView` over a `GroupModel` (several groups
  can be selected) beside a `FolderListView` over a `FolderModel`, which shows the selected
  groups' folders and is painted by `FolderDelegate`. Folders are dragged onto a group with
  the `GroupListView::folderMimeType` drag, which `FolderListView::startDrag` starts itself
  so that a Move drop never removes model rows. In manual order the same drag dropped within
  the folder list, and the `GroupListView::groupMimeType` drag within the group list, reorder
  the store (`Store::moveNextTo`, `moveGroupNextTo`); `ReorderDrop` finds the place and draws
  the style's drop indicator. "Clean all" and "Rescan all" act on the
  selected groups; Tools has the commands for every group. `FolderListView` derives from
  `CleanableListView`, which handles the Clean buttons and spinners of any model that answers
  `FolderModel`'s roles. `CleanDetailsDialog` has two tabs: Items, an `ItemsView` of the
  folder's last scan (Folder > Show what cleaning deletes, or a double click on the folder's
  row; Enter edits the folder), and Subfolders, a `SubfolderPanel` (Folder > Clean subfolders,
  Matching folders only), made when the tab is first shown; each menu opens its own tab. Its
  Clean button cleans the whole folder through `MainWindow::cleanFolder`, with the same
  question and `CleanChoice::method`, and is enabled under the same rule as the row's button
  (`CanCleanRole`). The dialog stays open and shows the rescan. `SubfolderPanel`
  lists a folder's first-level subfolders through `SubfolderModel`, which scans and cleans each
  one on its own thread pool, and asks `Monitor` to rescan the folder after each clean.
  For a Whole folder the Items tab's `ItemsView` is in summary mode: the space and the
  number of items in large type, without the list, which can hold thousands of items.
  `ItemsDialog` shows an `ItemsView` alone, for a subfolder or the confirmation box (Show
  items, offered only for a Matching folder). `DetectDialog` runs `Scanner::detect` in the background and returns the
  chosen types. The tray menu and tooltip are rebuilt from `Monitor::summaryChanged`.
  `GroupModel` and `FolderModel` sort their own rows (`SortOrder`: manual, name, or space to
  clean), so row numbers from `idAt` and `rowOf` stay valid; a re-sort is a layout change that
  keeps the selection. View > Sort groups by / Sort folders by picks the order, kept in
  QSettings beside the window geometry, and Move up and down work only in manual order. The
  toolbar's two sort buttons open the same submenus; their icons are drawn in code on every
  platform (`Icons::sortGroupsIcon`, `sortFoldersIcon`).
  `UsagePanel`, in the toolbar right of the buttons, paints the space to clean in every
  folder, plus in the selected groups when they are not all of them. `FreeSpaceDialog` (the Free space
  button) lists every disk that holds a monitored folder, with a check box that takes the disk
  out of the free space warning (`Settings::freeAlertOffDisks`); the status bar shows only the
  disks at a warning level, in the most serious level's colour. The UI shows only the
  space to clean, never a folder's total size; the warning limits (`Folder::warnBytes`,
  `Settings::cleanAlert`) also measure the space to clean.
- Warnings: `Settings::cleanAlert` (space to clean in all folders) and `freeAlert` (free space
  per disk) are each an `Alert` (`src/model/Alert.h`): three limits for the levels Low, Medium
  and High (0 turns a level off) and the level from which the tray notifies. Clean levels
  grow, free space levels shrink; `SettingsDialog` refuses levels out of order.
  `Monitor::warnings` gives each warning's level; a folder over its own `warnBytes` makes the
  clean level at least Low. The tray icon shows the clean level as a circle at the bottom left
  and the free level as a triangle at the bottom right, in `Icons::severityColor`.
  `MainWindow::warningReasons` says why, one line per low disk, the total and each large
  folder; the tray menu lists them at its top and the tooltip repeats them. A
  notification comes when a level rises to the notify level or above while the window is
  hidden. A file without `cleanAlert` or `freeAlert` keeps a single old limit
  (`totalWarnBytes`, `freeWarnBytes`) as the Low level, except the old free space default.
- `src/platform`: `SingleInstance`.
- Libraries: `dac_core` (model and cleaning) and `dac_ui` (widgets), so tests can link them.
  `main.cpp` and `src/platform` belong only to the `dev-asset-cleaner` executable.
- `tests`: `tst_store`, `tst_scanner` (builds folder trees in a temporary folder and cleans
  them), and `tst_mainwindow`, which drives the window on the offscreen platform.

## Things to keep in mind

- Safety comes first. `Scanner::unsafeWholeReason` refuses a Whole folder on a drive root, the
  home folder and folders above it; `Monitor::clean` checks it. Nothing falls back to
  deleting everything: a Matching folder without types deletes nothing, and detection only
  makes Matching folders. `removeTree` never follows symbolic links or junctions. Cleaning
  scans again before it deletes, so it never deletes from an old result.
- Cleaning moves to the trash by default (`CleanMethod`). The UI picks the method when the
  cleaning starts, through `CleanChoice::method`: Delete when `Settings::deleteDirectly` is on
  or Shift is held, except during the Clean shortcuts (`ignoreShiftInShortcuts`). Items that
  `SystemFolders::isInTrash` places in a trash are always deleted, and a failed move never
  falls back to deleting. Tests that clean must use `CleanMethod::Delete` or set
  `deleteDirectly`, so that nothing reaches the real trash.
- A folder's `excludes` names first-level subfolders that `scan`, `scanWhole` and `detect`
  size but never match or delete, for every kind; `ExcludeDialog` edits them.
- Built-in types are code (`BuiltInTypes::all`), not data. Custom types are stored; a stored
  type with a built-in id is ignored. "All programming languages" is the union of the other
  built-in rules, and a test checks that.
- Missing JSON fields must keep old data loading: new fields need defaults in `fromJson` and
  `Store::load`. Sizes are stored as JSON numbers (doubles), exact up to 2^53 bytes.
- Sizes are file sizes in decimal units (`Format::bytes`), not disk blocks; hard links count
  once per link.
- The folder filter matches paths as well as names. Tests that filter must use text that
  cannot occur in a temporary folder name.
- On macOS, Qt's `Ctrl` is the Command key and `Meta` is Control.
- The app icon is drawn in `src/ui/Icons.cpp`. `resources/dev-asset-cleaner.ico` and `.icns`
  are generated from it by the `make_icons` target and committed; regenerate them after
  changing the drawing. The tray's warning icon adds up to two badges to the same drawing.
- `docs/screenshot-macos.png` and `docs/screenshot.png` (Windows) are drawn with made-up data
  by the `make_screenshot` target, through `Monitor::setResultForDisplay` and
  `setVolumesForDisplay`. Each is made on its own platform. The tool captures the real window
  with its title bar: with `PrintWindow` on Windows, and with ScreenCaptureKit on macOS
  (`tools/WindowCapture_mac.mm`; `screencapture -l` fails on macOS 27), which needs the Screen
  Recording permission. Claude Code's shell is refused that permission; run the tool from the
  Terminal panel.
- Qt 6.8 crashed on macOS 27 in two places, worked around as in favdir and kept with Qt 6.10.
  `TrayIcon` uses a native `NSStatusItem` instead of `QSystemTrayIcon` (fixed in Qt 6.12); its
  menu is rebuilt from the `QMenu` each time it opens. `Icons::toolIcon` draws the toolbar
  icons instead of using `QIcon::fromTheme` (not yet checked with Qt 6.10). Check both after
  upgrading Qt.
- On macOS, hiding the window to the menu bar makes the app an accessory app (no Dock icon)
  and showing it makes it a regular app again, through `TrayIcon::setDockIconVisible`.
- "Start when I sign in" lives in the OS, not in the JSON file: the per-user Run key on
  Windows (the same value the installer writes) and `SMAppService.mainApp` on macOS
  (`LoginItem`).
- Dialogs run with `exec()` sit at AppKit's modal panel level, above other apps and the
  Screenshot controls, and Qt (6.8 to 6.10) leaves them there. `MacApp::attachModalDialogs`
  keeps each modal dialog at the normal level and attaches it to its owner window as a child
  window, as Mirante does, so it stays directly above that window.
- Qt's macOS style (6.8 to 6.10) draws no usable check boxes in item views on macOS 27: at
  most a misplaced box on the first row. `ItemCheckStyle` replaces its
  `PE_IndicatorItemViewItemCheck` drawing, as in favdir; drawing after the style showed both. `ItemCheckStyle::apply`
  gives a view a style that paints them in code; use it on every view with checkable items.

## Packaging and releases

- `cmake --install` deploys the Qt runtime next to the app; the prefix must be absolute.
- `cpack -C Release` in a Windows build folder makes the NSIS setup, with the same patched
  NSIS template and finish page options as favdir. Configuring fails if the template's
  anchors change in a new CMake version.
- On macOS the bundle is `Dev Asset Cleaner.app`, from the executable's `OUTPUT_NAME`
  (`DAC_BUNDLE_NAME`); the target and the Windows executable keep the name
  `dev-asset-cleaner`. Quote paths to the bundle: the name has spaces. The data folder does not
  change with it, because `main.cpp` sets the application name.
- `cpack -C Release` in a macOS build folder makes the DragNDrop disk image. The install step
  signs the bundle after macdeployqt. `DAC_CODESIGN_IDENTITY` picks the identity: `-` (ad hoc,
  the default) or a Developer ID, which adds the hardened runtime and timestamp.
- `scripts/release-macos.sh <version> [--upload]` (bash) makes the notarised disk image, the
  same way as the sibling project dreame-control: the Developer ID Application identity found
  in the keychain (or `DAC_SIGN_IDENTITY`), and the shared notarytool keychain profile
  `mac-apps`, made from an App Store Connect API key (or the `DAC_NOTARY_KEY*` variables, which
  CI sets). It reads notarytool's JSON status, prints Apple's log on failure and retries
  stapling. The release job builds a temporary keychain from the `release` environment's secrets and exports
  those variables. That job holds secrets: keep its actions pinned to commits.
- `.github/workflows/build.yml` builds and tests on every push and pull request, except when
  only documentation changed (`*.md` files and `docs/`). Its `ci` job passes when the build
  passed or was skipped, and it is the only check that branch protection requires. Pull
  request runs are titled with the PR number. `release.yml` runs when a
  tag `vX.Y.Z` is pushed: it publishes the GitHub release (unless it exists), builds with that
  version (`-DDAC_VERSION`) and attaches the Windows setup and the macOS disk image. The tag
  is the version; nothing in the tree is bumped for a release.

## Public repository

The repository is public. Never commit local paths (drive letters, user folders, temp
folders) in code, docs or test data.
