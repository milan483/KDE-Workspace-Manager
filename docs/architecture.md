# Architecture

## Product boundary

A workspace profile describes applications and their windows, window geometry and state, virtual-desktop membership, and optional application-specific session data. Switching profiles must not require closing applications that belong to the profile being left.

The primary target is KDE Plasma on Wayland. X11 is not a primary target.

## Component boundaries

- **Core** — shared build metadata and foundational types that do not belong to a feature subsystem.
- **Application shell** — process entry point, KDE application metadata, translation setup, and QML engine initialization.
- **UI** — Qt 6 / Kirigami pages and navigation. It presents application state and delegates behavior to C++ services as those services are introduced.
- **Profiles** — an independent Qt Core library for the domain model, versioned JSON serialization, validation, storage, and schema migrations.
- **KWin integration** — a packaged JavaScript adapter that captures KWin-managed windows and virtual desktops.
- **Runtime composition** — constructs the application-level services and injects concrete platform adapters without exposing those dependencies to the application shell.
- **D-Bus bridge** — a versioned session-level request queue between the native application and the KWin adapter.
- **Applications** — KIO-based launching and optional application-specific session adapters.
- **Restoration** — preflight, desktop preparation, application launch, window matching, state application, and post-restore verification.

These are architectural boundaries, not a claim that all components have been implemented.

## Application shell

The native executable initializes KDE application metadata and localization, then loads the embedded QML module through QQmlApplicationEngine. The QML module uses Kirigami.ApplicationWindow and a GlobalDrawer for page navigation.

The desktop entry uses the reverse-DNS identity org.kde.kdeworkspacemanager. The same identity is assigned to the application at runtime so desktop integration and Wayland window identity remain consistent.

The Workspaces page exposes profile listing, capture preview, save, update, rename, duplicate, and delete operations. The Settings page remains a placeholder. Restore and Switch actions are intentionally absent.

Keep UI presentation in QML and application initialization in src/app. Do not put KWin APIs, D-Bus transport, persistence, or restoration behavior in the application shell.

## Workspace orchestration

The application-level service layer lives in `src/workspaces/`:

- **WorkspaceService** coordinates profile operations, capture lifecycle, preview confirmation, validation, and persistence.
- **ProfileListModel** exposes profile names and desktop/application/window counts to QML.
- **WorkspaceCaptureBackend** is the KWin-neutral injectable capture boundary implemented by `KWinCaptureClient`. The service consumes its signals and does not perform D-Bus or KWin communication itself.
- The runtime composition factory constructs the ProfileRepository-backed WorkspaceService and injects the concrete KWin capture adapter. The application shell calls this factory through a KWin-neutral API and exposes the service to QML as `workspaceService`.

Saving and updating are two-phase operations: capture produces a validated preview; only explicit user confirmation persists it. The existing ProfileRepository remains responsible for atomic storage. Updating preserves the profile UUID and matching logical desktop UUIDs by captured desktop order, preserves launch specifications for uniquely matched application identities, and replaces the captured application/window snapshot. Ambiguous launch-metadata matches fail safely rather than guessing. Duplication creates new profile, desktop, application, and window UUIDs and remaps desktop references.

This layer does not launch applications, create or rearrange runtime virtual desktops, restore windows, or switch profiles.

## Profile Engine

The Profile Engine is built as `kwm_profiles` with the public alias `KDEWorkspaceManager::Profiles`. It depends on Qt Core only and is independent of the application shell, Kirigami, KWin, and D-Bus.

Its responsibilities are:
- Domain types for profiles, logical desktops, applications, launch specifications, windows, matching identity, geometry, desktop membership, and window state.
- V2 JSON serialization and deserialization, with an explicit V1-to-V2 migration.
- Structural and semantic validation with structured errors.
- A sequential schema-migration entry point. V1 is the first persisted format; no v0 migration is defined.
- Persistent profile listing, loading, upsert, and removal.

The repository stores one JSON file per profile under `QStandardPaths::AppDataLocation/profiles`. Filenames use canonical profile UUIDs. Writes use `QSaveFile` with direct-write fallback disabled. Listing preserves healthy profiles when another file is corrupt and reports the affected file separately.

The V2 schema uses stable logical UUIDs, collection-based desktop membership, explicit all-desktop membership, separate application launch specifications, and distinct minimized, fullscreen, and four-mode maximization state. V1 profiles migrate explicitly by mapping the former maximized boolean to full or none. Geometry is normalized relative to the available work area; V2 remains single-monitor.

V1 does not serialize application-specific session data; adding such data requires an explicit future schema change. The Profile Engine does not enumerate windows, launch applications, communicate with KWin, resolve runtime window matches, or provide profile-management UI. See [Profile Format](profile-format.md) for the persisted contract.

## Window identity

Temporary KWin window UUIDs are runtime handles and must not be persisted as profile identity. Matching should prefer desktopFileName, use resourceClass as a fallback, and treat role or other attributes as optional discriminators. Window titles are not primary identity. Ambiguous matches must be reported rather than guessed.

## Virtual desktops

Profiles use stable logical desktop identifiers that are mapped to runtime KWin desktop identifiers. A window may belong to multiple desktops or all desktops; the profile model must preserve that distinction.

The selected architecture assigns each profile a dedicated logical group of virtual desktops. Applications remain running when the active profile changes.

## Restoration ordering constraint

Native testing on Plasma/KWin 6.8.3 Wayland confirmed that setting `frameGeometry` for a window on an inactive virtual desktop may only partially apply a requested rectangle (the controlled test moved the window but did not resize it). Activating that desktop before setting geometry allowed an exact readback. The restoration engine must therefore activate each target desktop before applying its window geometry, verify the result, and restore the intended active desktop after the operation. This behavior must be revalidated on other supported Plasma versions.

## Current implementation

The current build contains kwm_core, the independent kwm_profiles library, the kwm_workspaces service/model library, the kwm_kwin capture adapter library, a runtime composition library, a native Kirigami application shell, an embedded QML module, KDE desktop metadata, and a packaged KWin JavaScript adapter. The KWin library provides the D-Bus request queue/service, script lifecycle controller, KWin capture client implementing the workspace capture interface, and snapshot-to-profile parser. The packaged script uses KWin 6.8.3 scripting APIs verified against the live Plasma Wayland session. Unit tests cover the queue, parser, workspace service, and profile list model; an optional native integration test exercises live capture in a running KWin session. Capture remains read-only and does not alter existing windows or desktops.

Application launching, restoration orchestration, and workspace switching remain future implementation units.
