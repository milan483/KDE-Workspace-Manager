# Project instructions

## Project boundaries

KDE Workspace Manager is a native KDE Plasma workspace-profile application. Keep core profile/domain logic separate from KWin integration, application launching, and the Kirigami UI.

## Layout

- `src/core/`: shared C++ core types and logic.
- `src/profiles/`: planned profile persistence, validation, and migrations.
- `src/kwin/` and `kwin-script/`: planned KWin adapter and integration.
- `src/applications/`: planned application launching and app-specific adapters.
- `src/restoration/`: planned restore orchestration.
- `src/ui/`: planned Kirigami interface.
- `tests/unit/`: Qt Test unit tests.
- `docs/`: architecture and developer documentation.
- `.github/workflows/ci.yml`: build and test CI.

## Build and verification

The current build requires a C++20 compiler, CMake 3.24+, Ninja, and Qt 6.4+ development files for Core and Test.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Design constraints

- Keep KWin-specific APIs and D-Bus transport out of `src/core/`.
- Persist stable logical identifiers, not temporary KWin window UUIDs.
- Prefer `desktopFileName` for window matching, fall back to `resourceClass`, and use other attributes only as discriminators. Surface ambiguous matches instead of moving an uncertain window.
- Model virtual-desktop membership as a collection and represent all-desktop/sticky windows explicitly.
- Keep maximized, fullscreen, and minimized state distinct.
- Version the JSON profile format and provide explicit migrations when its schema changes.
- Add or update Qt Test coverage for behavioral changes.
