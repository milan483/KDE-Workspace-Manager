# KDE Workspace Manager

A native KDE Plasma application for saving, restoring, and switching between complete working environments without closing the applications belonging to the workspace being left.

## Project status

The repository foundation and native Qt 6 / Kirigami application shell are in place. Unit 3 adds the independent Profile Engine.

The application currently provides:
- A Kirigami desktop window with a navigation drawer.
- An initial Workspaces page with an explicit empty state.
- A Settings placeholder page.
- KDE application metadata and desktop integration.

The Profile Engine provides:
- A versioned V1 JSON profile model.
- Structural and semantic validation.
- JSON serialization and deserialization.
- A sequential schema-migration entry point.
- Persistent profile CRUD with atomic writes.

Profile-management UI, KWin integration, window enumeration and capture, application launching, restoration, and workspace switching are not implemented yet. The UI does not expose nonfunctional workspace actions.

See [Profile Format](docs/profile-format.md) for the persisted schema and storage contract.

## Profile contents

A workspace profile can represent:
- Applications and their windows.
- Window geometry and distinct maximized, fullscreen, and minimized states.
- Stable logical virtual desktops and multi-desktop membership.
- Explicit all-desktop/sticky windows.
- Separate application launch specifications and window-matching identities.

The primary target is KDE Plasma on Wayland. X11 is not a primary target.

## Architecture

- C++20, Qt 6, Kirigami, and KDE Frameworks 6.
- A Qt Core-only Profile Engine library, independent of the UI and KWin.
- Versioned JSON profile storage and explicit migration infrastructure.
- A planned KWin JavaScript adapter with session D-Bus communication.
- KIO for planned application launching.
- CMake, Ninja, Qt Test, CTest, and GitHub Actions.

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
