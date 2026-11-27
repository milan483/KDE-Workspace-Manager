# Architecture

## Product boundary

A workspace profile describes applications and their windows, window geometry and state, virtual-desktop membership, and optional application-specific session data. Switching profiles must not require closing applications that belong to the profile being left.

The primary target is KDE Plasma on Wayland. X11 is not a primary target.

## Component boundaries

- **Core** — shared build metadata and foundational types that do not belong to a feature subsystem.
- **Application shell** — process entry point, KDE application metadata, translation setup, and QML engine initialization.
- **UI** — Qt 6 / Kirigami pages and navigation. It presents application state and delegates behavior to C++ services as those services are introduced.
- **Profiles** — an independent Qt Core library for the domain model, versioned JSON serialization, validation, storage, and schema migrations.
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

## Profile Engine

The Profile Engine is built as `kwm_profiles` with the public alias `KDEWorkspaceManager::Profiles`. It depends on Qt Core only and is independent of the application shell, Kirigami, KWin, and D-Bus.

Its responsibilities are:
- Domain types for profiles, logical desktops, applications, launch specifications, windows, matching identity, geometry, desktop membership, and window state.
- V1 JSON serialization and deserialization.
- Structural and semantic validation with structured errors.
- A sequential schema-migration entry point. V1 is the first persisted format; no v0 migration is defined.
- Persistent profile listing, loading, upsert, and removal.

The repository stores one JSON file per profile under `QStandardPaths::AppDataLocation/profiles`. Filenames use canonical profile UUIDs. Writes use `QSaveFile` with direct-write fallback disabled. Listing preserves healthy profiles when another file is corrupt and reports the affected file separately.

The V1 schema uses stable logical UUIDs, collection-based desktop membership, explicit all-desktop membership, separate application launch specifications, and distinct minimized, maximized, and fullscreen state. Geometry is normalized relative to the available work area; V1 is single-monitor.

V1 does not serialize application-specific session data; adding such data requires an explicit future schema change. The Profile Engine does not enumerate windows, launch applications, communicate with KWin, resolve runtime window matches, or provide profile-management UI. See [Profile Format](profile-format.md) for the persisted contract.

## Window identity

Temporary KWin window UUIDs are runtime handles and must not be persisted as profile identity. Matching should prefer desktopFileName, use resourceClass as a fallback, and treat role or other attributes as optional discriminators. Window titles are not primary identity. Ambiguous matches must be reported rather than guessed.

## Virtual desktops

Profiles use stable logical desktop identifiers that are mapped to runtime KWin desktop identifiers. A window may belong to multiple desktops or all desktops; the profile model must preserve that distinction.

The selected architecture assigns each profile a dedicated logical group of virtual desktops. Applications remain running when the active profile changes.

## Restoration ordering constraint

Phase 0 testing found that changing geometry for a window on an inactive virtual desktop did not take effect in the initial experiment, while geometry changes on the active desktop succeeded. The restoration engine must account for this ordering constraint and verify behavior on the target Plasma version before the restore sequence is finalized.

## Current implementation

The current build contains kwm_core, the independent kwm_profiles library, a native Kirigami application shell, an embedded QML module, KDE desktop metadata, and automated checks for core metadata, profile model/serialization/validation/migration/repository behavior, GUI startup/navigation, QML lint, and desktop-entry validity.

The KWin adapter, D-Bus protocol, snapshot capture, application launching, restoration engine, and profile-management UI remain future implementation units.
