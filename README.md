# Dev Asset Cleaner

A small desktop app that watches how much space your folders use and cleans them with one
click. It knows what programming tools leave behind, such as Rust's `target` folders,
`node_modules` and CMake build folders, and deletes only those. It runs on Windows and macOS.

![The Dev Asset Cleaner window on macOS](docs/screenshot-macos.png)

- Folders are kept in **groups**, listed on the left of the window. The app starts with one
  group, **Default**, which can be renamed or deleted like any other. Select several groups
  (Ctrl-click or Shift-click) to see their folders together. Deleting a group stops monitoring
  its folders; nothing on disk is deleted.
- The folder list shows the folders of the selected groups. Each row shows how much cleaning
  would free and how much the folder holds, with a **Clean** button. The toolbar cleans the
  selected folder or all folders of the selected groups; **Tools > Clean everything** cleans
  every group.
- Drag a folder onto a group to move it there, or use **Move to group** in its menu. A folder
  path can be in one group only. Folders dropped from Explorer or Finder onto a group are added
  to that group.
- Each folder has one or more **cleaner types**, which say what cleaning deletes (see below).
  New folders use **Folder contents**.
- **Detect types in a folder** searches a folder and lists the cleaner types that apply to it,
  with what each would free. Check the ones to keep, and the folder is added with them, or an
  existing folder switches to them.
- The **Add** button also offers the tool caches it finds on this computer, such as the Go
  module cache, the npm cache, Gradle's caches and Xcode's derived data.
- **Add system folders** lists the operating system's trash and cache folders that are safe to
  empty: the Recycle Bin of each drive, Windows error reports, crash dumps and graphics shader
  caches on Windows; the Trash, the trash of other volumes and crash reports on macOS; the
  freedesktop.org trash and thumbnails elsewhere. Browser caches and broad cache folders such
  as `~/Library/Caches` are left out, since running programs use them. Reading the macOS Trash
  needs Full Disk Access.
- Folders are scanned in the background when added, and again every 30 minutes.
- Closing the window keeps the app in the system tray (the menu bar on macOS, where it also
  leaves the Dock until the window shows again). The tray menu and the tooltip show the space
  each folder uses and the free space on their disks. With several groups, the tray menu has a
  submenu for each group.
- The tray icon gets a yellow badge when free space is low, when a folder is over its own
  size limit, or when all folders together are over a total limit. Set the limits in Settings
  and in each folder.
- Settings can **start the app when you sign in** (on macOS 13 and later). It then starts
  hidden in the tray or menu bar.

## Cleaner types

A cleaner type says what cleaning a folder deletes. There are two kinds:

- **Everything in the folder.** Cleaning deletes everything inside the folder, but keeps the
  folder itself. The built-in **Folder contents** type works this way. Use it for caches and
  download folders. For safety, it refuses to clean a drive's root, your home folder or a folder
  above it.
- **Matching folders.** Cleaning deletes only the subfolders, at any depth, that match one of
  the type's rules. Use it for a folder of projects.

A rule has a folder name and, optionally, the names of files that must sit next to that folder.
For example, Rust's rule is `target` next to `Cargo.toml`, so a `target` folder in a project
without `Cargo.toml` is left alone. Both names take the wildcards `*` and `?`, and ignore case.
A matched folder is deleted as a whole and not searched further.

The built-in types:

| Type | Deletes |
|---|---|
| Folder contents | Everything in the folder |
| Rust | `target` next to `Cargo.toml` |
| C and C++ (CMake) | `build`, `build-*`, `cmake-build-*` and `out` next to `CMakeLists.txt` |
| JavaScript and TypeScript | `node_modules`, `.next`, `.nuxt`, `.turbo`, `.parcel-cache` and `.svelte-kit` next to `package.json` |
| Python | `__pycache__`, `.pytest_cache`, `.mypy_cache` and `.ruff_cache` anywhere; `.tox`, `.venv` and `venv` next to a Python project file |
| Java and Kotlin (Gradle, Maven) | `build` and `.gradle` next to a Gradle build file; `target` next to `pom.xml` |
| .NET | `bin` and `obj` next to a `.csproj`, `.fsproj` or `.vbproj` file |
| Go | `bin` next to `go.mod` |
| Swift | `.build` next to `Package.swift` |
| Dart and Flutter | `build` and `.dart_tool` next to `pubspec.yaml` |
| Zig | `.zig-cache`, `zig-cache` and `zig-out` next to `build.zig` |
| Haskell | `.stack-work` next to `stack.yaml`; `dist-newstyle` next to a Cabal file |
| All programming languages | Every rule above |

To find which types fit a folder, use **File > Detect types in a folder** (or **Detect
types** in a folder's right-click menu). It walks the folder once with the rules of every type
that matches folders, and lists each type that found something, with its folders and their
size. "All programming languages" is listed too, but starts unchecked, since it repeats the
others. Detection covers programming build outputs and dependencies today; other kinds of
findings can be added to it later.

Add your own types in **Tools > Cleaner types**. A built-in type cannot be changed, but
**Duplicate** makes an editable copy of it. A folder can use several types at once, for example
Rust and JavaScript.

Read-only files and folders, such as those in Go's module cache, are made writable and then
deleted. Symbolic links are deleted, never followed.

Before it deletes anything, the app asks, and **Show items** lists what will go. Cleaning scans
the folder again first, so it deletes what is there at that moment. Deleted files do not go to
the trash: cleaning is meant to free space, and it cannot be undone.

## Keyboard

| Key | Effect |
|---|---|
| Type | Filter by name or path |
| Enter | Edit the selected folder, or rename the selected group |
| Ctrl+K | Clean the selected folder |
| Ctrl+Shift+K | Clean all folders of the selected groups |
| Ctrl+R | Rescan the selected folder |
| Ctrl+Shift+R, F5 | Rescan all folders of the selected groups |
| Ctrl+I | Show what cleaning the folder deletes |
| Ctrl+O | Open the folder in Explorer or Finder |
| Ctrl+C | Copy the folder's path |
| Ctrl+Shift+Up, Ctrl+Shift+Down | Move the folder up or down |
| Ctrl+Alt+Up, Ctrl+Alt+Down | Move the group up or down |
| Ctrl+N | Add a folder to the current group |
| Ctrl+Shift+N | Add a group |
| Ctrl+D | Detect the cleaner types of a folder |
| F2 | Edit the folder |
| Delete | Stop monitoring the folder (nothing is deleted) |
| Esc | Clear the filter, or hide the window |

On macOS, Ctrl in this table means ⌘.

You can also drag folders from Explorer or Finder onto the list to add them.

## Data

Groups and their folders, custom types and settings are saved as JSON:

- Windows: `%APPDATA%\dev-asset-cleaner\dev-asset-cleaner\dev-asset-cleaner.json`
- macOS: `~/Library/Application Support/dev-asset-cleaner/dev-asset-cleaner/dev-asset-cleaner.json`

The environment variable `DAC_DATA_FILE` points the app at another file, which is useful for
testing. A file from a version without groups opens with its folders in a **Default** group.

## Building

### What you need

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio 2022 or its Build Tools, with "Desktop development with C++" | Xcode Command Line Tools (`xcode-select --install`) |
| Tools | CMake 3.25 or later | CMake 3.25 or later and Ninja (`brew install cmake ninja`) |
| Qt download | [uv](https://docs.astral.sh/uv/) | [uv](https://docs.astral.sh/uv/) |

### Steps

1. Download Qt. The script fetches the prebuilt Qt 6.10.3 base module (about 300 MB) into
   `.qt/` with [aqtinstall](https://github.com/miurahr/aqtinstall). It needs no Qt account and
   runs only once.

   To share one Qt between projects, set the `QT_ROOT` environment variable to a folder in
   aqtinstall's layout (`<root>/6.10.3/msvc2022_64` or `<root>/6.10.3/macos`) before you run the
   script and configure. The script then uses Qt from there, or downloads it there.

   ```sh
   scripts/setup.ps1     # Windows (PowerShell)
   sh scripts/setup.sh   # macOS
   ```

2. Configure, build and test. Use the preset `windows` or `macos`.

   ```sh
   cmake --preset windows
   cmake --build --preset windows
   ctest --preset windows
   ```

   The app is in `build/windows/bin/Release/dev-asset-cleaner.exe`, or
   `build/macos/bin/Release/Dev Asset Cleaner.app`.

3. Package. This copies the app and the Qt libraries it needs into one folder. The prefix must
   be an absolute path.

   ```sh
   cmake --install build/windows --config Release --prefix "$PWD/dist"
   ```

The executable icon comes from `resources/dev-asset-cleaner.ico` and
`resources/dev-asset-cleaner.icns`. Both are generated from the drawing in `src/ui/Icons.cpp`;
after changing it, rebuild them with `cmake --build --preset windows --target make_icons`, then
run `build/windows/bin/Release/make_icons resources`.

The screenshot above is `docs/screenshot-macos.png`, drawn with made-up folders by the
`make_screenshot` target. After changing the look of the window, rebuild it with
`cmake --build --preset macos --target make_screenshot`, then run
`build/macos/bin/Release/make_screenshot docs/screenshot-macos.png`. On Windows, use the
`windows` preset and `docs/screenshot.png`. On macOS the tool captures the window with
`screencapture`, which needs the Screen Recording permission; without it, the picture has no
title bar.

### Tasks

`Taskfile.yml` wraps the steps above for [Task](https://taskfile.dev). Each task uses the
preset for the current platform. Run `task --list` to see them all.

```sh
task test         # download Qt if needed, configure, build and test
task run          # start the app with a scratch data file in the build folder
task run-prod     # start the app with your real data file
task install      # copy the app and the Qt libraries into dist/
task package      # make the Windows setup or the macOS disk image
task icons        # regenerate the icon files
task screenshot   # regenerate the screenshot of the current platform
```

### Windows setup

With [NSIS](https://nsis.sourceforge.io/) installed, `cpack` builds
`dev-asset-cleaner-<version>-win64.exe` from a Release build:

```sh
cd build/windows
cpack -C Release
```

The setup installs the app with the Qt and Visual C++ runtime libraries it needs, adds a Start
menu entry, and on its last page offers to run the app and to start it when you sign in. It
then starts hidden in the tray (`dev-asset-cleaner --hidden`). The same option is in Settings.
When you install a new version over an old one, the sign-in option keeps its current setting.

### macOS disk image

`cpack` builds `dev-asset-cleaner-<version>-macos.dmg` from a Release build:

```sh
cd build/macos
cpack -C Release
```

The disk image holds `Dev Asset Cleaner.app`, with the Qt libraries it needs, and a link to the
Applications folder. Drag the app onto the link to install it.

This disk image is signed ad hoc, not with an Apple Developer ID, and it is not notarised. The
first time you start a downloaded copy, macOS refuses to open it. Open System Settings > Privacy &
Security and click **Open Anyway** next to the message about the app.

### Notarised macOS disk image

`scripts/release-macos.sh` builds a disk image that macOS opens without a warning. It builds and
tests the app in `build/macos-release`, signs the app and the disk image with your Developer ID,
has Apple notarise the disk image, and staples the notarisation ticket to it.

Locally it uses the "Developer ID Application" identity in your keychain and the notarytool
keychain profile `mac-apps`. Make that profile once, from an App Store Connect API key:

```sh
xcrun notarytool store-credentials mac-apps --key AuthKey_<key id>.p8 --key-id <key id> --issuer <issuer id>
```

Then build the disk image. `--upload` attaches it to the GitHub release `v<version>` and replaces
the one there:

```sh
bash scripts/release-macos.sh 1.0.0 --upload
```

The header of the script lists the environment variables that override these defaults.

### Releases

Pushing a tag `vX.Y.Z` runs the release workflow. It publishes a GitHub release for the tag,
builds and tests the app with that version, then attaches the Windows setup and the notarised
macOS disk image to the release. The macOS job runs `scripts/release-macos.sh` with these
secrets of the `release` environment:

| Secret | Value |
|---|---|
| `MACOS_CERTIFICATE_P12` | The "Developer ID Application" certificate with its private key, exported from Keychain Access as a `.p12` file, base64-encoded: `base64 -i cert.p12 \| pbcopy` |
| `MACOS_CERTIFICATE_PASSWORD` | The password given to the `.p12` file on export |
| `NOTARY_API_KEY` | An App Store Connect API key: the contents of its `.p8` file. Make one at appstoreconnect.apple.com under Users and Access > Integrations > Team Keys, with the Developer role. |
| `NOTARY_API_KEY_ID` | That key's ID |
| `NOTARY_API_ISSUER` | The issuer ID shown above the list of keys |

Without these secrets the macOS job fails rather than attach a disk image that is not
notarised. Limit the environment's deployment branches and tags to `v*` tags and `master`, so
other branches cannot use the secrets.

To change the Qt version, edit the version at the top of both setup scripts and in
`CMakePresets.json`.

## License

Dev Asset Cleaner is released under the [MIT License](LICENSE).
