# Project instructions

## Project boundaries

KDE Workspace Manager is a native KDE Plasma workspace-profile application. Keep core profile/domain logic separate from the application shell, UI, KWin integration, application launching, and restoration orchestration.

## Layout

- src/core/: shared C++ core types and logic.
- src/app/: native application entry point and QML module target.
- src/ui/: Kirigami QML window and pages.
- src/profiles/: independent Qt Core Profile Engine library (domain model, JSON serialization, validation, storage, and migrations).
- src/kwin/: KWin D-Bus bridge, script lifecycle controller, capture client, and snapshot parser.
- kwin-script/: packaged KWin JavaScript capture adapter.
- src/applications/: planned application launching and app-specific adapters.
- src/restoration/: planned restore orchestration.
- tests/unit/: Qt Test unit tests.
- docs/: architecture, development, and profile-format documentation.
- .github/workflows/ci.yml: build and test CI.

## Build and verification

The current build requires a C++20 compiler, CMake 3.24+, Ninja, Extra CMake Modules 6.0+, Qt 6.8+ (Core, Gui, Qml, Quick, QuickControls2, Widgets, DBus, Test), KDE Frameworks 6.8+ (CoreAddons, I18n, IconThemes, Kirigami, QQC2DesktopStyle), qmllint, desktop-file-validate, AppStream CLI, and Qt QPA plugins (including offscreen).

From the repository root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$PWD/build/stage"
```

## Design constraints

- Keep KWin-specific APIs and D-Bus transport out of src/core, src/profiles, and src/app.
- Keep QML presentation in src/ui; do not add domain or restoration behavior to QML.
- Do not expose Save, Restore, Capture, or Switch actions until they are functional.
- Use org.kde.kdeworkspacemanager consistently for the desktop-file and runtime application identity.
- Persist stable logical identifiers, not temporary KWin window UUIDs.
- Prefer desktopFileName for window matching, fall back to resourceClass, and use other attributes only as discriminators. Surface ambiguous matches instead of moving an uncertain window.
- Model virtual-desktop membership as a collection and represent all-desktop/sticky windows explicitly.
- Keep application launch specifications separate from window-matching identity.
- Preserve `none`, vertical, horizontal, and full maximization separately from fullscreen and minimized state; minimized windows retain their underlying state.
- Version the JSON profile format and provide explicit sequential migrations when its schema changes. Do not invent a legacy v0 schema.
- Keep the Profile Engine independent of KWin, D-Bus, Kirigami, and the application UI.
- Keep KWin APIs and D-Bus transport in `src/kwin/` and `kwin-script/`; the adapter must not persist runtime window UUIDs.
- Capture geometry only when a normal/restored rectangle is available. KWin 6.8.3 does not expose `geometryRestore` to scripts, so omit geometry for maximized/fullscreen windows.
- The current profile format is single-monitor; reject multi-monitor capture rather than silently losing monitor placement.
- Before applying geometry to a window on an inactive virtual desktop, activate the target desktop, apply and verify geometry, then restore the intended active desktop.
- Add or update automated tests for behavioral changes.
- Do not add a project license until the user selects one.
