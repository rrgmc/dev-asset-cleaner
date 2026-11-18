# Dev Asset Cleaner

A Qt 6 Widgets app (C++20, CMake) for Windows and macOS that watches the space folders use and
deletes build outputs and caches in them. `README.md` describes the features and the build for
users. The build, packaging, release and platform code come from the sibling project favdir;
keep fixes to those parts in step with it.

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
  scratch file. Never point a test or a trial run at a real folder with "Folder contents": it
  deletes everything in it.

## Layout

- `src/model`: `CleanerType` (rules and the built-in types), `Folder`, `Store` (JSON
  persistence, debounced atomic saves). No widgets.
- `src/clean`: `Scanner` (walks a folder, finds what its types match, deletes it; `detect`
  reports per type what each rule-based type would delete, for `DetectDialog`),
  `Monitor` (runs scans and cleans on a thread pool, keeps results, free space and warnings),
  `KnownCaches` (tool caches offered in the Add menu), `SystemFolders` (the OS's trash and
  cache folders that are safe to empty, offered by `SystemFoldersDialog`; keep browser caches
  and broad folders such as `~/Library/Caches` out of it).
- `src/ui`: widgets. `MainWindow` holds one `FolderListView` over a `FolderModel`, painted by
  `FolderDelegate`. `DetectDialog` runs `Scanner::detect` in the background and returns the
  chosen types. The tray menu and tooltip are rebuilt from `Monitor::summaryChanged`.
- `src/platform`: `SingleInstance`.
- Libraries: `dac_core` (model and cleaning) and `dac_ui` (widgets), so tests can link them.
  `main.cpp` and `src/platform` belong only to the `dev-asset-cleaner` executable.
- `tests`: `tst_store`, `tst_scanner` (builds folder trees in a temporary folder and cleans
  them), and `tst_mainwindow`, which drives the window on the offscreen platform.

## Things to keep in mind

- Safety comes first. `Scanner::unsafeReason` refuses "Folder contents" on a drive root, the
  home folder and folders above it; `Monitor::clean` checks it. `removeTree` never follows
  symbolic links or junctions. Cleaning scans again before it deletes, so it never deletes from
  an old result.
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
  changing the drawing. The tray's warning icon adds a badge to the same drawing.
- `docs/screenshot-macos.png` is drawn with made-up data by the `make_screenshot` target,
  through `Monitor::setResultForDisplay` and `setVolumesForDisplay`. On macOS the tool captures
  the real window with `screencapture`, which needs the Screen Recording permission.
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
- Dialogs run with `exec()` sit at AppKit's modal panel level, and Qt (6.8 to 6.10) does not lower them
  when the app goes to the background, so they floated over other apps.
  `MacApp::lowerModalWindowsWhenInactive` lowers them while another app is active.
- Qt 6.8's macOS style drew no check boxes in item views on macOS 27 (not yet checked with Qt
  6.10). `ItemCheckStyle::apply`
  gives a view a style that paints them in code; use it on every view with checkable items.

## Packaging and releases

- `cmake --install` deploys the Qt runtime next to the app; the prefix must be absolute.
- `cpack -C Release` in a Windows build folder makes the NSIS setup, with the same patched
  NSIS template and finish page options as favdir. Configuring fails if the template's
  anchors change in a new CMake version.
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
- `.github/workflows/build.yml` builds and tests on every push. `release.yml` runs when a
  tag `vX.Y.Z` is pushed: it publishes the GitHub release (unless it exists), builds with that
  version (`-DDAC_VERSION`) and attaches the Windows setup and the macOS disk image. The tag
  is the version; nothing in the tree is bumped for a release.

## Public repository

The repository is public. Never commit local paths (drive letters, user folders, temp
folders) in code, docs or test data.
