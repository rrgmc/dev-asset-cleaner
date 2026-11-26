# Changelog

All notable changes to this project are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.5.0] - 2026-11-26

### Changed

- On macOS, the menu bar icon stays black and white when there is a warning: only the warning
  badges are in colour.

## [1.4.0] - 2026-11-26

### Changed

- The warnings in the tray menu and tooltip show the current amount beside the limit it passed:
  the space to clean in all folders, in each large folder, and the free space on each low disk.
- On macOS the folder filter sits above the folder list, as on Windows, so the toolbar fits in
  a normal window width.

### Fixed

- Showing the window from another virtual desktop (Windows) or Space (macOS) brings it and its
  dialogs to the current one, instead of leaving them out of sight. On Windows, a tray click
  shows a window that is on another desktop instead of hiding it.

[Unreleased]: https://github.com/rrgmc/dev-asset-cleaner/compare/v1.5.0...HEAD
[1.5.0]: https://github.com/rrgmc/dev-asset-cleaner/compare/v1.4.0...v1.5.0
[1.4.0]: https://github.com/rrgmc/dev-asset-cleaner/compare/v1.3.0...v1.4.0
