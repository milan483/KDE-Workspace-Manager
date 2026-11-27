# KDE Workspace Manager

A native KDE Plasma application for saving, restoring, and switching between complete working environments without closing the applications belonging to the workspace being left.

## Project status

Early development. Phase 0 technical feasibility has been validated on KDE Plasma Wayland. Phase 1 is establishing the native application foundation, profile format, KWin integration boundary, automated tests, and CI.

The current build contains a small Qt Core library and its unit tests. The Kirigami application, profile engine, KWin adapter, and snapshot capture are not implemented yet.

## Planned capabilities

Workspace profiles describe:

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
- Qt 6.4+ development files (Core and Test)

### Configure, build, and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

See [Development](docs/development.md) for prerequisites and build options, and [Architecture](docs/architecture.md) for the component boundaries and known integration constraints.
