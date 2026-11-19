# Dev Asset Cleaner

Programming tools leave large folders behind: Rust's `target`, `node_modules`, CMake build
folders, package caches. Dev Asset Cleaner keeps track of how much space they take and frees it
with one click. It runs on Windows and macOS.

![The Dev Asset Cleaner window on macOS](docs/screenshot-macos.png)

![The Dev Asset Cleaner window on Windows](docs/screenshot.png)

## Install

Download the latest version from the
[releases page](https://github.com/rrgmc/dev-asset-cleaner/releases):

- **Windows:** run `dev-asset-cleaner-<version>-win64.exe`. On its last page, the setup can
  start the app when you sign in.
- **macOS:** open `dev-asset-cleaner-<version>-macos.dmg` and drag the app onto the
  Applications folder.

## Getting started

1. Click **Add** and choose what to watch:
   - **Matching folders** for a folder of projects. The app searches it, shows which kinds of
     project it found and how much each would free, and adds the folder with the ones you keep.
   - **Folder** for a folder whose whole contents can go, such as a cache or a downloads folder.
   - A tool cache it found on this computer, such as the npm, Go or Gradle cache.
   - **Add system folders** for the Recycle Bin or Trash and other system caches that are safe
     to empty.
2. Each row shows how much space cleaning would free. Click its **Clean** button.
3. The app asks first, and **Show items** lists what will be removed.

Close the window and the app keeps running in the system tray (the menu bar on macOS). It
scans your folders again every 30 minutes.

## What gets deleted

Each folder is one of two kinds, chosen when you add it:

- **Matching folders:** only the build outputs and dependencies of the projects inside are
  deleted, at any depth. Your source files stay.
- **Whole folder:** everything inside is deleted, but the folder itself stays. The app refuses
  to do this to a drive's root, your home folder or any folder above it.

To keep a project untouched, add it to the folder's **excludes** in the folder's settings.

Cleaning moves items to the trash (the Recycle Bin on Windows), so you can undo it. The space
is freed when you empty the trash. To delete at once, hold Shift when you click Clean, or turn
on **Cleaning deletes directly** in Settings.

### Recognised projects

- Rust
- C and C++ (CMake)
- JavaScript and TypeScript
- Python
- Java and Kotlin (Gradle, Maven)
- .NET
- Go
- Swift
- Dart and Flutter
- Zig
- Haskell

Only build outputs and dependencies inside a project are deleted, so a `target` folder outside
a Rust project is left alone. To add your own rules, open **Tools > Cleaner types**.

## More features

- **Groups.** Sort your folders into groups, shown on the left. Drag a folder onto a group to
  move it. Select several groups to see and clean their folders together.
- **See what goes.** Double-click a folder, or choose **Folder > Show what cleaning deletes**,
  to list the items from its last scan. For a folder that is emptied whole, it shows only the
  space and the number of items. The window's **Clean** button cleans the folder and then shows
  what is left.
- **One project at a time.** **Folder > Clean subfolders** lists each project in a folder of
  projects, with its own Clean button.
- **Warnings.** Two warnings watch the space to clean in all folders and the free space on each
  disk. Each has three levels, Low, Medium and High, shown in yellow, orange and red. Set where
  each level starts, and the level from which you get a notification, in Settings. The tray
  icon shows the space to clean as a circle on the left and free space as a triangle on the
  right, in the colour of the level, and its menu lists the reasons at the top. A folder can
  also have its own limit. To leave a folder out of the warnings, edit it and clear **Include
  this folder in the warnings**; the list then tags it "not in warnings", and the toolbar adds
  the space to clean in the folders that count. **Free space** shows
  the free space on each disk; uncheck a disk there to leave it out of the warning.
- **Sorting.** Order groups and folders by name, by space to clean, or in your own order by
  dragging them.
- **Start when I sign in.** Turn it on in Settings (macOS 13 and later). The app then starts
  hidden in the tray.

## Good to know

- On macOS, reading the Trash needs Full Disk Access.
- An item that cannot be moved to the trash, for example on a network drive, is left in place
  and reported.
- Your groups, folders and settings are saved in
  `%APPDATA%\dev-asset-cleaner\dev-asset-cleaner\dev-asset-cleaner.json` on Windows and
  `~/Library/Application Support/dev-asset-cleaner/dev-asset-cleaner/dev-asset-cleaner.json`
  on macOS.

## Building from source

See [BUILDING.md](BUILDING.md).

## Author

Rangel Reale (realerangel@gmail.com)

## License

Dev Asset Cleaner is released under the [MIT License](LICENSE).
