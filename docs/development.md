# Development

## Prerequisites

The current application shell and Profile Engine require:

- A C++20 compiler.
- CMake 3.24 or newer.
- Ninja.
- Extra CMake Modules 6.0 or newer.
- Qt 6.8 or newer development files: Core, Gui, Qml, Quick, QuickControls2, Widgets, and Test.
- KDE Frameworks 6.8 or newer development files: CoreAddons, I18n, IconThemes, Kirigami, and QQC2DesktopStyle.
- The QML runtime modules for Kirigami, KI18n, and the KDE desktop style.
- qmllint and desktop-file-validate for the full test suite.
- AppStream CLI and Qt QPA platform plugins, including the offscreen plugin, for KDE's install-manifest check and headless GUI test.

Package names differ by distribution. On Ubuntu 26.04, the CI job installs the corresponding Qt and KF6 development packages, Extra CMake Modules, desktop-file-utils, and the QML lint tools.

## Configure

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
```

## Build

```sh
cmake --build build --parallel
```

## Run tests

```sh
ctest --test-dir build --output-on-failure
```

The test suite covers:
- Core build metadata.
- Profile domain invariants, JSON serialization/deserialization, structural and semantic validation, schema-version handling, and repository CRUD/failure cases.
- Offscreen application startup and navigation.
- QML linting and desktop-entry validation.
- KDE's AppStream install-manifest check.

Profile repository tests inject a temporary directory. They do not access the real application-data profile directory. See [Profile Format](profile-format.md) for the schema and persistence contract.

## Staged installation

```sh
cmake --install build --prefix "$PWD/build/stage"
```

This verifies the install layout without modifying the system. The GitHub Actions workflow also checks the staged executable and desktop-entry paths.

## CI

GitHub Actions uses the Ubuntu 26.04 hosted runner and installs the Qt 6 / KDE Frameworks 6 development dependencies required by the application shell and Profile Engine. The workflow runs configure, build, CTest, and staged-install checks for pushes and pull requests.
