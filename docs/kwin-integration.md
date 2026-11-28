# KWin integration

## Status and boundary

The repository contains a working KWin capture backend connected to user-confirmed Save and Update workflows through `WorkspaceService`. The Workspaces page also exposes profile preview and CRUD operations. Application launching, window restoration, and workspace switching are not implemented; the UI does not expose nonfunctional Restore or Switch controls.

The integration is isolated from the Profile Engine:

- `src/kwin/` contains the C++ D-Bus bridge service, request queue, script controller, capture client implementing the workspace capture interface, and snapshot parser.
- `src/workspaces/` contains the application-level service and QML profile-list model. It coordinates capture and Profile Engine operations without owning KWin or D-Bus transport.
- `kwin-script/` contains the KPackage metadata and JavaScript adapter loaded by KWin.
- `src/profiles/` remains Qt Core-only and has no KWin or D-Bus dependency.

## Runtime protocol

The bridge uses session D-Bus:

| Item | Value |
|---|---|
| Service | `org.kde.kdeworkspacemanager.KWinBridge` |
| Object path | `/org/kde/kdeworkspacemanager/KWinBridge` |
| Interface | `org.kde.kdeworkspacemanager.KWinBridge1` |
| Protocol version | `1` |

The C++ request queue exposes two methods to the KWin script:

- `takeNextRequest()` returns an empty string when the queue is empty. An active request is returned repeatedly until it is completed, so a transient script/D-Bus interruption does not silently lose a read-only capture request.
- `completeRequest(requestId, responseJson)` acknowledges the active request and delivers its response to the C++ capture client. A mismatched request ID is rejected. The C++ client allows one outstanding capture at a time and cancels it after 15 seconds if KWin does not respond or the script stops.

Requests contain `protocol_version`, `request_id`, and `operation`. Version 1 currently supports the `capture` operation. The KWin script polls asynchronously through `callDBus()`; it does not execute shell commands or accept arbitrary method names from the request. The C++ script controller loads and runs the script through KWin's `/Scripting` D-Bus interface and unloads it when stopped. A stale copy of the same plugin is unloaded before a new controller instance starts, preventing duplicate pollers after an application restart.

## Captured data

The adapter captures:

- Ordered virtual desktops, their runtime UUIDs and names, and the active desktop.
- Managed normal application windows and dialogs. Special windows, popup windows, and windows without a stable application identity or desktop membership are excluded and counted.
- `desktopFileName` as the preferred application identity, falling back to `resourceClass`.
- `windowRole` when non-empty.
- Specific multi-desktop membership or explicit all-desktop/sticky membership.
- Minimized, fullscreen, and four-mode maximization state. Maximize mode is captured independently of fullscreen so any flags KWin retains underneath fullscreen are preserved.
- Normal window geometry normalized against `workspace.clientArea(KWin.MaximizeArea, window)` when it is available and fully contained in the work area.

Runtime KWin desktop UUIDs are mapped to newly generated logical profile UUIDs by the C++ parser. KWin window UUIDs, process IDs, and window titles are not persisted as profile identity.

The response includes counts for excluded windows, omitted normal geometry, and windows whose maximized/fullscreen state prevents capture of their underlying normal geometry.

## Geometry limitations

KWin 6.8.3 exposes `frameGeometry` but does not expose `geometryRestore` to KWin JavaScript. For maximized or fullscreen windows, `frameGeometry` describes the current maximized/fullscreen rectangle, not the hidden normal restore rectangle. The adapter therefore omits geometry for those windows instead of storing a misleading rectangle. Their maximize/fullscreen state is still captured.

The current profile format is single-monitor. The adapter rejects capture when KWin reports more than one screen rather than silently discarding monitor identity. It also omits normal geometry for windows outside the work area instead of clamping their coordinates or silently changing their layout.

Native testing on Plasma/KWin 6.8.3 Wayland also established a restoration ordering constraint. In a controlled-window test, assigning a new rectangle on an inactive desktop changed its position but left its original size unchanged. After activating the target desktop, setting `frameGeometry` applied the complete requested rectangle exactly. A future restore operation must activate each target desktop before applying geometry, verify the readback, and restore the intended active desktop at the end.

## Native verification

The live environment used for the Unit 4 spike was Plasma 6.8.3, KWin 6.8.3, and Wayland.

Verified behaviors:

- `workspace.windowList()` enumerates managed windows; the adapter filters special and popup windows.
- Application identity, role, frame geometry, maximize mode, minimized/fullscreen state, sticky state, and desktop memberships are readable.
- `workspace.clientArea(KWin.MaximizeArea, window)` returns the work area used for normalized geometry.
- Inactive-desktop geometry writes did not apply the complete requested rectangle (position changed, size did not); activating the target desktop before writing applied the complete rectangle and allowed the original active desktop to be restored.
- A real Qt D-Bus service and KWin script exchanged an asynchronous request and callback through `callDBus()`.
- The production C++ bridge, script controller, JavaScript adapter, and snapshot parser completed a live capture and unloaded the script and bridge service afterward.
- The controller also recovered from a previously loaded, non-running copy of the same script before performing capture.
- A controlled normal Qt window was captured with normalized geometry.

The optional live integration test is disabled by default because it requires an active KWin session. Enable it with:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_KWIN_INTEGRATION_TESTS=ON
cmake --build build --parallel
ctest --test-dir build -R kwm_kwin_live_capture --output-on-failure
```

The test performs a read-only capture. It does not move existing windows or change virtual desktops.

## Remaining work

- Populate safe application launch specifications through a separate desktop-entry resolution task; the capture parser must not infer commands from window classes.
- Decide how to preserve or explicitly report normal restore geometry for maximized/fullscreen windows if a future KWin API or safe source becomes available.
- Implement application launching and restoration orchestration.
- Implement the inactive-desktop activation/geometry/readback sequence in restoration, with failure recovery and final active-desktop restoration.
- Add multi-monitor identity and geometry conversion before claiming multi-monitor support.
