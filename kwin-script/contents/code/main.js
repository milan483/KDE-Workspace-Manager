var KWM_PROTOCOL_VERSION = 1;
var KWM_BRIDGE_SERVICE = "org.kde.kdeworkspacemanager.KWinBridge";
var KWM_BRIDGE_PATH = "/org/kde/kdeworkspacemanager/KWinBridge";
var KWM_BRIDGE_INTERFACE = "org.kde.kdeworkspacemanager.KWinBridge1";

var kwmRequestCallInFlight = false;
var kwmProcessingRequest = false;
var kwmPollCallToken = 0;
var kwmCompletionCallToken = 0;

var kwmPollTimeout = new QTimer(workspace);
kwmPollTimeout.interval = 5000;
kwmPollTimeout.timeout.connect(function() {
    if (kwmRequestCallInFlight) {
        kwmPollCallToken++;
        kwmRequestCallInFlight = false;
    }
    kwmPollTimeout.stop();
});

var kwmCompletionTimeout = new QTimer(workspace);
kwmCompletionTimeout.interval = 5000;
kwmCompletionTimeout.timeout.connect(function() {
    if (kwmProcessingRequest) {
        kwmCompletionCallToken++;
        kwmProcessingRequest = false;
    }
    kwmCompletionTimeout.stop();
});

function kwmErrorResponse(requestId, code, message) {
    return {
        protocol_version: KWM_PROTOCOL_VERSION,
        request_id: requestId,
        operation: "capture",
        error: {
            code: code,
            message: message
        }
    };
}

function kwmMaximizeModeName(value) {
    switch (Number(value)) {
    case 0:
        return "none";
    case 1:
        return "vertical";
    case 2:
        return "horizontal";
    case 3:
        return "full";
    default:
        return null;
    }
}

function kwmCaptureSnapshot() {
    var screenCount = workspace.screens.length;
    if (screenCount !== 1) {
        return {
            error: {
                code: "unsupported_screen_count",
                message: "KDE Workspace Manager capture currently supports exactly one monitor."
            }
        };
    }

    var desktops = workspace.desktops;
    if (!desktops || desktops.length === 0 || !workspace.currentDesktop) {
        return {
            error: {
                code: "virtual_desktops_unavailable",
                message: "KWin did not expose a usable virtual desktop layout."
            }
        };
    }

    var desktopRecords = [];
    for (var desktopIndex = 0; desktopIndex < desktops.length; desktopIndex++) {
        var desktop = desktops[desktopIndex];
        desktopRecords.push({
            id: String(desktop.id),
            name: String(desktop.name)
        });
    }

    var snapshot = {
        screen_count: screenCount,
        active_desktop_id: String(workspace.currentDesktop.id),
        virtual_desktops: desktopRecords,
        windows: [],
        excluded_window_count: 0,
        geometry_omitted_count: 0,
        state_geometry_unavailable_count: 0
    };

    var windows = workspace.windowList();
    for (var windowIndex = 0; windowIndex < windows.length; windowIndex++) {
        var window = windows[windowIndex];

        if (!window.managed || window.deleted || window.specialWindow || window.popupWindow ||
            (!window.normalWindow && !window.dialog)) {
            snapshot.excluded_window_count++;
            continue;
        }

        var identityKind = null;
        var identityValue = null;
        if (typeof window.desktopFileName === "string" && window.desktopFileName.trim().length > 0) {
            identityKind = "desktop_file_name";
            identityValue = window.desktopFileName;
        } else if (typeof window.resourceClass === "string" && window.resourceClass.trim().length > 0) {
            identityKind = "resource_class";
            identityValue = window.resourceClass;
        }
        if (!identityKind) {
            snapshot.excluded_window_count++;
            continue;
        }

        var maximizeMode = kwmMaximizeModeName(window.maximizeMode);
        if (maximizeMode === null) {
            return {
                error: {
                    code: "unsupported_maximize_mode",
                    message: "KWin returned an unsupported maximize mode for a managed application window."
                }
            };
        }

        var allDesktops = Boolean(window.onAllDesktops);
        var desktopIds = [];
        if (!allDesktops) {
            var memberships = window.desktops;
            for (var membershipIndex = 0; membershipIndex < memberships.length; membershipIndex++) {
                desktopIds.push(String(memberships[membershipIndex].id));
            }
            if (desktopIds.length === 0) {
                snapshot.excluded_window_count++;
                continue;
            }
        }

        var state = {
            minimized: Boolean(window.minimized),
            maximize_mode: maximizeMode,
            fullscreen: Boolean(window.fullScreen)
        };

        var record = {
            application_identity: {
                kind: identityKind,
                value: identityValue
            },
            all_desktops: allDesktops,
            state: state
        };
        if (!allDesktops) {
            record.desktop_ids = desktopIds;
        }
        if (typeof window.windowRole === "string" && window.windowRole.trim().length > 0) {
            record.role = window.windowRole;
        }

        if (maximizeMode === "none" && !state.fullscreen) {
            var area = workspace.clientArea(KWin.MaximizeArea, window);
            var frame = window.frameGeometry;
            if (area && area.width > 0 && area.height > 0 && frame.width > 0 && frame.height > 0) {
                var geometry = {
                    x: (frame.x - area.x) / area.width,
                    y: (frame.y - area.y) / area.height,
                    width: frame.width / area.width,
                    height: frame.height / area.height
                };
                if (isFinite(geometry.x) && isFinite(geometry.y) &&
                    isFinite(geometry.width) && isFinite(geometry.height) &&
                    geometry.x >= 0 && geometry.y >= 0 && geometry.width > 0 && geometry.height > 0 &&
                    geometry.x + geometry.width <= 1 && geometry.y + geometry.height <= 1) {
                    record.geometry = geometry;
                } else {
                    snapshot.geometry_omitted_count++;
                }
            } else {
                snapshot.geometry_omitted_count++;
            }
        } else {
            snapshot.state_geometry_unavailable_count++;
        }

        snapshot.windows.push(record);
    }

    return {snapshot: snapshot};
}

function kwmCompleteRequest(request, response) {
    var callToken = ++kwmCompletionCallToken;
    kwmProcessingRequest = true;
    kwmCompletionTimeout.start();
    callDBus(
        KWM_BRIDGE_SERVICE,
        KWM_BRIDGE_PATH,
        KWM_BRIDGE_INTERFACE,
        "completeRequest",
        request.request_id,
        JSON.stringify(response),
        function(accepted) {
            if (callToken !== kwmCompletionCallToken) {
                return;
            }
            kwmCompletionTimeout.stop();
            kwmProcessingRequest = false;
            if (!accepted) {
                print("KWM bridge rejected a capture response.");
            }
        }
    );
}

function kwmHandleRequest(requestJson) {
    var request;
    try {
        request = JSON.parse(requestJson);
    } catch (error) {
        return;
    }

    if (!request || typeof request.request_id !== "string" || request.request_id.length === 0) {
        return;
    }

    if (request.protocol_version !== KWM_PROTOCOL_VERSION) {
        kwmCompleteRequest(request, kwmErrorResponse(request.request_id, "unsupported_protocol_version",
                                                      "The request uses an unsupported bridge protocol version."));
        return;
    }
    if (request.operation !== "capture") {
        kwmCompleteRequest(request, kwmErrorResponse(request.request_id, "unsupported_operation",
                                                      "The requested KWin operation is not supported."));
        return;
    }

    var capture = kwmCaptureSnapshot();
    if (capture.error) {
        kwmCompleteRequest(request, kwmErrorResponse(request.request_id, capture.error.code, capture.error.message));
        return;
    }

    kwmCompleteRequest(request, {
        protocol_version: KWM_PROTOCOL_VERSION,
        request_id: request.request_id,
        operation: "capture",
        snapshot: capture.snapshot
    });
}

function kwmPollBridge() {
    if (kwmRequestCallInFlight || kwmProcessingRequest) {
        return;
    }

    var callToken = ++kwmPollCallToken;
    kwmRequestCallInFlight = true;
    kwmPollTimeout.start();
    callDBus(
        KWM_BRIDGE_SERVICE,
        KWM_BRIDGE_PATH,
        KWM_BRIDGE_INTERFACE,
        "takeNextRequest",
        function(requestJson) {
            if (callToken !== kwmPollCallToken) {
                return;
            }
            kwmPollTimeout.stop();
            kwmRequestCallInFlight = false;
            if (typeof requestJson !== "string" || requestJson.length === 0) {
                return;
            }
            kwmHandleRequest(requestJson);
        }
    );
}

var kwmPollTimer = new QTimer(workspace);
kwmPollTimer.interval = 250;
kwmPollTimer.timeout.connect(kwmPollBridge);
kwmPollTimer.start();
