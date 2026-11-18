# Dev Asset Cleaner

A small desktop app that watches how much space your folders use and cleans them with one
click. It knows what programming tools leave behind, such as Rust's `target` folders,
`node_modules` and CMake build folders, and deletes only those. It runs on Windows and macOS.

![The Dev Asset Cleaner window on macOS](docs/screenshot-macos.png)

- The window lists your folders. Each row shows how much cleaning would free and how much the
  folder holds, with a **Clean** button. The toolbar cleans the selected folder or all of
  them.
- Each folder has one or more **cleaner types**, which say what cleaning deletes (see below).
  New folders use **Folder contents**.
- The **Add** button also offers the tool caches it finds on this computer, such as the Go
  module cache, the npm cache, Gradle's caches and Xcode's derived data.
- Folders are scanned in the background when added, and again every 30 minutes.
- Closing the window keeps the app in the system tray (the menu bar on macOS, where it also
  leaves the Dock until the window shows again). The tray menu and the tooltip show the space
  each folder uses and the free space on their disks.
- The tray icon gets an orange badge when free space is low, when a folder is over its own
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
| Enter | Edit the selected folder |
| Ctrl+K | Clean the selected folder |
| Ctrl+Shift+K | Clean all folders |
| Ctrl+R | Rescan the selected folder |
| Ctrl+Shift+R, F5 | Rescan all folders |
| Ctrl+I | Show what cleaning the folder deletes |
| Ctrl+O | Open the folder in Explorer or Finder |
| Ctrl+C | Copy the folder's path |
| Ctrl+Shift+Up, Ctrl+Shift+Down | Move the folder up or down |
| Ctrl+N | Add a folder |
| F2 | Edit the folder |
| Delete | Stop monitoring the folder (nothing is deleted) |
| Esc | Clear the filter, or hide the window |

On macOS, Ctrl in this table means ⌘.

You can also drag folders from Explorer or Finder onto the list to add them.

## Data

Folders, custom types and settings are saved as JSON:

- Windows: `%APPDATA%\dev-asset-cleaner\dev-asset-cleaner\dev-asset-cleaner.json`
- macOS: `~/Library/Application Support/dev-asset-cleaner/dev-asset-cleaner/dev-asset-cleaner.json`

The environment variable `DAC_DATA_FILE` points the app at another file, which is useful for
testing.

## Building

### What you need

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio 2022 or its Build Tools, with "Desktop development with C++" | Xcode Command Line Tools (`xcode-select --install`) |
| Tools | CMake 3.25 or later | CMake 3.25 or later and Ninja (`brew install cmake ninja`) |
| Qt download | [uv](https://docs.astral.sh/uv/) | [uv](https://docs.astral.sh/uv/) |

### Steps

1. Download Qt. The script fetches the prebuilt Qt 6.8.3 base module (about 270 MB) into
   `.qt/` with [aqtinstall](https://github.com/miurahr/aqtinstall). It needs no Qt account and
   runs only once.

   To share one Qt between projects, set the `QT_ROOT` environment variable to a folder in
   aqtinstall's layout (`<root>/6.8.3/msvc2022_64` or `<root>/6.8.3/macos`) before you run the
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
   `build/macos/bin/Release/dev-asset-cleaner.app`.

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

The disk image holds `dev-asset-cleaner.app`, with the Qt libraries it needs, and a link to the
Applications folder. Drag the app onto the link to install it.

This disk image is signed ad hoc, not with an Apple Developer ID, and it is not notarised. The
first time you start a downloaded copy, macOS refuses to open it. Open System Settings > Privacy &
Security and click **Open Anyway** next to the message about the app.

### Notarised macOS disk image

`scripts/release-macos.sh` builds a disk image that macOS opens without a warning. It runs on a
Mac with a "Developer ID Application" certificate in the keychain. It builds and tests the app
in `build/macos-release`, signs the app and the disk image with the certificate, has Apple
notarise the disk image, and staples the notarisation ticket to it.

Store your notarisation credentials once. The password is an app-specific password from
[account.apple.com](https://account.apple.com):

```sh
xcrun notarytool store-credentials dev-asset-cleaner --apple-id <Apple ID> --team-id <team ID>
```

Then build the disk image. `--upload` attaches it to the GitHub release `v<version>` and replaces
the one there:

```sh
sh scripts/release-macos.sh 1.0.0 --upload
```

### Releases

Pushing a tag `vX.Y.Z` runs the release workflow. It publishes a GitHub release for the tag,
builds and tests the app with that version, then attaches the Windows setup and the notarised
macOS disk image to the release. The macOS job runs `scripts/release-macos.sh` with these
secrets of the `release` environment:

| Secret | Value |
|---|---|
| `MACOS_CERTIFICATE_P12` | The Developer ID Application certificate and its private key, exported as a `.p12` file, in base64 |
| `MACOS_CERTIFICATE_PASSWORD` | The password of the `.p12` file |
| `NOTARY_API_KEY` | The contents of an App Store Connect API key file (`AuthKey_<key ID>.p8`) |
| `NOTARY_API_KEY_ID` | The key ID of that API key |
| `NOTARY_API_ISSUER` | The issuer ID shown above the list of API keys in App Store Connect |

The environment should accept only `v*` tags and `master`, so other branches cannot use the
secrets.

To change the Qt version, edit the version at the top of both setup scripts and in
`CMakePresets.json`.

## License

Dev Asset Cleaner is released under the [MIT License](LICENSE).
