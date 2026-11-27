# Architecture

## Product boundary

A workspace profile describes applications and their windows, window geometry and state, virtual-desktop membership, and optional application-specific session data. Switching profiles must not require closing applications that belong to the profile being left.

The primary target is KDE Plasma on Wayland. X11 is not a primary target.

## Component boundaries

- **Core** — stable domain types, profile identity, and logic that does not depend on KWin or the UI.
- **Application shell** — process entry point, KDE application metadata, translation setup, and QML engine initialization.
- **UI** — Qt 6 / Kirigami pages and navigation. It presents application state and delegates behavior to C++ services as those services are introduced.
- **Profiles** — versioned JSON serialization, validation, storage, and schema migrations.
- **KWin integration** — a JavaScript adapter that observes and manipulates KWin-managed windows and desktops.
- **D-Bus service** — the session-level interface between the native application and the KWin adapter.
- **Applications** — KIO-based launching and optional application-specific session adapters.
- **Restoration** — preflight, desktop preparation, application launch, window matching, state application, and post-restore verification.

These are architectural boundaries, not a claim that all components have been implemented.

## Application shell

The native executable initializes KDE application metadata and localization, then loads the embedded QML module through QQmlApplicationEngine. The QML module uses Kirigami.ApplicationWindow and a GlobalDrawer for page navigation.

The desktop entry uses the reverse-DNS identity org.kde.kdeworkspacemanager. The same identity is assigned to the application at runtime so desktop integration and Wayland window identity remain consistent.

The current Workspaces page is an explicit empty state. The Settings page is a placeholder. Neither page exposes operations that are not implemented.

Keep UI presentation in QML and application initialization in src/app. Do not put KWin APIs, D-Bus transport, persistence, or restoration behavior in the application shell.

## Window identity

Temporary KWin window UUIDs are runtime handles and must not be persisted as profile identity. Matching should prefer desktopFileName, use resourceClass as a fallback, and treat role or other attributes as optional discriminators. Window titles are not primary identity. Ambiguous matches must be reported rather than guessed.

## Virtual desktops

Profiles use stable logical desktop identifiers that are mapped to runtime KWin desktop identifiers. A window may belong to multiple desktops or all desktops; the profile model must preserve that distinction.

The selected architecture assigns each profile a dedicated logical group of virtual desktops. Applications remain running when the active profile changes.

## Restoration ordering constraint

Phase 0 testing found that changing geometry for a window on an inactive virtual desktop did not take effect in the initial experiment, while geometry changes on the active desktop succeeded. The restoration engine must account for this ordering constraint and verify behavior on the target Plasma version before the restore sequence is finalized.

## Current implementation

The current build contains kwm_core, a native Kirigami application shell, an embedded QML module, KDE desktop metadata, and automated checks for core metadata, GUI startup/navigation, QML lint, and desktop-entry validity.

The profile schema and storage, KWin adapter, D-Bus protocol, snapshot capture, application launching, and restoration engine remain future implementation units.
