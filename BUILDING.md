# Building Dev Asset Cleaner

This page is for people who want to build the app from source or make a release. To use the
app, download it from the [releases page](https://github.com/rrgmc/dev-asset-cleaner/releases)
and read the [README](README.md).

## What you need

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio 2022 or its Build Tools, with "Desktop development with C++" | Xcode Command Line Tools (`xcode-select --install`) |
| Tools | CMake 3.25 or later | CMake 3.25 or later and Ninja (`brew install cmake ninja`) |
| Qt download | [uv](https://docs.astral.sh/uv/) | [uv](https://docs.astral.sh/uv/) |

## Steps

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

The screenshots in the README are `docs/screenshot-macos.png` and `docs/screenshot.png`, drawn with
made-up folders by the `make_screenshot` target. After changing the look of the window, rebuild
it with `cmake --build --preset macos --target make_screenshot`, then run
`build/macos/bin/Release/make_screenshot docs/screenshot-macos.png`. On Windows, use the
`windows` preset and `docs/screenshot.png`. On Windows the tool captures the window with
`PrintWindow`. On macOS it uses ScreenCaptureKit, because `screencapture -l` no longer captures
a single window on macOS 27. The app that runs the tool, such as the terminal, needs the Screen
Recording permission; without it, the picture has no title bar.

## Tasks

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

## Windows setup

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

## macOS disk image

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

## Notarised macOS disk image

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

## Releases

Pushing a tag `vX.Y.Z` runs the release workflow. It publishes a GitHub release for the tag,
builds and tests the app with that version, then attaches the Windows setup and the notarised
macOS disk image to the release. The tag is the version: nothing in the tree is bumped. A
local build without `-DDAC_VERSION` takes its version from the newest `vX.Y.Z` tag reachable
from the checked-out commit, or 0.0.0 when there is none. Help > About shows the version.

The macOS job runs `scripts/release-macos.sh` with these
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

## Trying a build without your data

The environment variable `DAC_DATA_FILE` points the app at another data file than your own.
Set it to a scratch file to try a build without touching your folders and settings.
`task run` does this for you.
