# Project instructions

## Project boundaries

KDE Workspace Manager is a native KDE Plasma workspace-profile application. Keep core profile/domain logic separate from the application shell, UI, KWin integration, application launching, and restoration orchestration.

## Layout

- src/core/: shared C++ core types and logic.
- src/app/: native application entry point and QML module target.
- src/ui/: Kirigami QML window and pages.
- src/profiles/: planned profile persistence, validation, and migrations.
- src/kwin/ and kwin-script/: planned KWin adapter and integration.
- src/applications/: planned application launching and app-specific adapters.
- src/restoration/: planned restore orchestration.
- tests/unit/: Qt Test unit tests.
- docs/: architecture and developer documentation.
- .github/workflows/ci.yml: build and test CI.

## Build and verification

The current build requires a C++20 compiler, CMake 3.24+, Ninja, Extra CMake Modules 6.0+, Qt 6.8+ (Core, Gui, Qml, Quick, QuickControls2, Widgets, Test), KDE Frameworks 6.8+ (CoreAddons, I18n, IconThemes, Kirigami, QQC2DesktopStyle), qmllint, desktop-file-validate, AppStream CLI, and Qt QPA plugins (including offscreen).

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$PWD/build/stage"
```

## Design constraints

- Keep KWin-specific APIs and D-Bus transport out of src/core and src/app.
- Keep QML presentation in src/ui; do not add domain or restoration behavior to QML.
- Do not expose Save, Restore, Capture, or Switch actions until they are functional.
- Use org.kde.kdeworkspacemanager consistently for the desktop-file and runtime application identity.
- Persist stable logical identifiers, not temporary KWin window UUIDs.
- Prefer desktopFileName for window matching, fall back to resourceClass, and use other attributes only as discriminators. Surface ambiguous matches instead of moving an uncertain window.
- Model virtual-desktop membership as a collection and represent all-desktop/sticky windows explicitly.
- Keep maximized, fullscreen, and minimized state distinct.
- Version the JSON profile format and provide explicit migrations when its schema changes.
- Add or update automated tests for behavioral changes.
- Do not add a project license until the user selects one.
