# KDE Workspace Manager

A native KDE Plasma application for saving, restoring, and switching between complete working environments without closing the applications belonging to the workspace being left.

## Project status

Early development. Phase 0 technical feasibility has been validated on KDE Plasma Wayland. Phase 1 establishes the application, profile format, KWin integration boundary, automated tests, and CI.

## Planned capabilities

Workspace profiles describe:

- Applications and their windows.
- Window geometry and display state.
- Virtual desktop assignments and membership.
- Application-specific session data where supported.
- Desktop configuration associated with each profile.

## Planned architecture

- C++ with Qt 6, Kirigami, and KDE Frameworks 6.
- KWin JavaScript adapter with session D-Bus communication.
- KIO for application launching.
- Versioned JSON profile storage.
- CMake and Ninja.
- Qt Test, CTest, and GitHub Actions.

The primary target is KDE Plasma on Wayland. X11 is not a primary target.

## Development

The project is not yet buildable. Build prerequisites, commands, architecture details, and contribution guidance will be documented as the Phase 1 foundation is implemented.

## Scope boundary

The initial implementation focuses on repository and application foundations, profile management, and workspace snapshot capture. Full workspace restoration and switching will follow after the KWin integration and restoration ordering have been validated.
