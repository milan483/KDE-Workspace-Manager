# KDE Workspace Manager

A native KDE Plasma application for saving, restoring, and switching between complete working environments without closing the applications belonging to the workspace being left.

## Project status

Early development. The repository foundation is in place and Unit 2 introduces the native Qt 6 / Kirigami application shell.

The current application provides:
- A Kirigami desktop window with a navigation drawer.
- An initial Workspaces page with an explicit empty state.
- A Settings placeholder page.
- KDE application metadata and desktop integration.

Workspace profile management, profile persistence, KWin integration, window capture, restoration, and switching are not implemented yet. The UI does not expose nonfunctional workspace actions.

## Planned capabilities

Workspace profiles will eventually describe:
- Applications and their windows.
- Window geometry, maximization, fullscreen, and minimized state.
- Virtual desktop assignments and multi-desktop membership.
- Application-specific session data where supported.
- Desktop configuration associated with each profile.

## Planned architecture

- C++20, Qt 6, Kirigami, and KDE Frameworks 6.
- KWin JavaScript adapter with session D-Bus communication.
- KIO for application launching.
- Versioned JSON profile storage and migrations.
- CMake and Ninja.
- Qt Test, CTest, and GitHub Actions.

The primary target is KDE Plasma on Wayland. X11 is not a primary target.

## Build

### Requirements

- C++20 compiler
- CMake 3.24+
- Ninja
- Extra CMake Modules 6.0+
- Qt 6.8+ development files: Core, Gui, Qml, Quick, QuickControls2, Widgets, and Test
- KDE Frameworks 6.8+ development files: CoreAddons, I18n, IconThemes, Kirigami, and QQC2DesktopStyle
- QML modules for Kirigami, KI18n, and the KDE desktop style
- `qmllint` and `desktop-file-validate` for the test suite

### Configure, build, and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Staged installation

```sh
cmake --install build --prefix "$PWD/build/stage"
```

This installs into the build directory rather than the system. See [Development](docs/development.md) for prerequisites and build options, and [Architecture](docs/architecture.md) for component boundaries and integration constraints.
