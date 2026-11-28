import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.ki18n

Kirigami.ScrollablePage {
    title: KI18n.i18n("Workspaces")

    function describeWindowState(windowInfo) {
        var descriptions = []
        if (windowInfo.minimized) {
            descriptions.push(KI18n.i18n("minimized"))
        }
        if (windowInfo.maximizeMode !== "none") {
            descriptions.push(KI18n.i18n("%1 maximized", windowInfo.maximizeMode))
        }
        if (windowInfo.fullscreen) {
            descriptions.push(KI18n.i18n("fullscreen"))
        }
        return descriptions.length > 0 ? descriptions.join(", ") : KI18n.i18n("normal")
    }

    actions: [
        Kirigami.Action {
            text: KI18n.i18n("Save Workspace")
            icon.name: "document-save"
            enabled: !workspaceService.busy && !workspaceService.hasPreview
            onTriggered: saveDialog.open()
        }
    ]

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Controls.Label {
            Layout.fillWidth: true
            visible: workspaceService.statusMessage.length > 0
            text: workspaceService.statusMessage
            wrapMode: Text.WordWrap
        }

        Controls.Label {
            Layout.fillWidth: true
            visible: workspaceService.errorMessage.length > 0
            text: workspaceService.errorMessage
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.WordWrap
        }

        Controls.Label {
            Layout.fillWidth: true
            visible: workspaceService.warningMessage.length > 0
            text: workspaceService.warningMessage
            color: Kirigami.Theme.neutralTextColor
            wrapMode: Text.WordWrap
        }

        Controls.Label {
            Layout.fillWidth: true
            visible: workspaceService.profileModel.count === 0
            text: KI18n.i18n("No saved workspaces yet. Use Save Workspace to capture the current desktop arrangement.")
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: workspaceService.profileModel

            delegate: Controls.Frame {
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        Controls.Label {
                            Layout.fillWidth: true
                            text: profileName
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Controls.Label {
                            text: KI18n.i18n("%1 desktops · %2 applications · %3 windows",
                                             desktopCount, applicationCount, windowCount)
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Button {
                            text: KI18n.i18n("Preview")
                            flat: true
                            enabled: !workspaceService.busy && !workspaceService.hasPreview
                            onClicked: {
                                const details = workspaceService.profileDetails(profileId)
                                if (Object.keys(details).length > 0) {
                                    profileDetailsDialog.profileInfo = details
                                    profileDetailsDialog.open()
                                }
                            }
                        }

                        Controls.Button {
                            text: KI18n.i18n("Update Workspace")
                            flat: true
                            enabled: !workspaceService.busy && !workspaceService.hasPreview
                            onClicked: workspaceService.updateWorkspace(profileId)
                        }

                        Controls.Button {
                            text: KI18n.i18n("Rename")
                            flat: true
                            enabled: !workspaceService.busy && !workspaceService.hasPreview
                            onClicked: {
                                renameDialog.profileId = profileId
                                renameField.text = profileName
                                renameDialog.open()
                            }
                        }

                        Controls.Button {
                            text: KI18n.i18n("Duplicate")
                            flat: true
                            enabled: !workspaceService.busy && !workspaceService.hasPreview
                            onClicked: workspaceService.duplicateProfile(profileId)
                        }

                        Controls.Button {
                            text: KI18n.i18n("Delete")
                            flat: true
                            enabled: !workspaceService.busy && !workspaceService.hasPreview
                            onClicked: {
                                deleteDialog.profileId = profileId
                                deleteDialog.profileName = profileName
                                deleteDialog.open()
                            }
                        }
                    }
                }
            }
        }

        Controls.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: workspaceService.busy
            visible: running
        }
    }

    Controls.Dialog {
        id: saveDialog
        modal: true
        title: KI18n.i18n("Save Workspace")
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel

        onAccepted: workspaceService.saveWorkspace(profileNameField.text)
        onOpened: profileNameField.forceActiveFocus()

        contentItem: ColumnLayout {
            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Choose a name for the current workspace profile.")
                wrapMode: Text.WordWrap
            }

            Controls.TextField {
                id: profileNameField
                Layout.fillWidth: true
                placeholderText: KI18n.i18n("Workspace name")
                onAccepted: saveDialog.accept()
            }
        }
    }

    Controls.Dialog {
        id: capturePreviewDialog
        modal: true
        title: workspaceService.previewIsUpdate
               ? KI18n.i18n("Review Workspace Update")
               : KI18n.i18n("Review Captured Workspace")
        width: Math.min(parent.width - Kirigami.Units.largeSpacing * 2, 560)

        footer: Controls.DialogButtonBox {
            standardButtons: Controls.DialogButtonBox.Save | Controls.DialogButtonBox.Cancel

            onAccepted: {
                if (workspaceService.confirmSavePreview()) {
                    capturePreviewDialog.close()
                }
            }

            onRejected: {
                workspaceService.cancelPreview()
                capturePreviewDialog.close()
            }
        }

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                text: workspaceService.preview.name || ""
                font.bold: true
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: workspaceService.errorMessage.length > 0
                text: workspaceService.errorMessage
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("%1 desktops · %2 applications · %3 windows",
                                 workspaceService.preview.desktopCount || 0,
                                 workspaceService.preview.applicationCount || 0,
                                 workspaceService.preview.windowCount || 0)
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Desktops: %1",
                                 (workspaceService.preview.desktopNames || []).join(", "))
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (workspaceService.preview.activeDesktopName || "").length > 0
                text: KI18n.i18n("Active desktop: %1", workspaceService.preview.activeDesktopName || "")
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (workspaceService.preview.excludedWindowCount || 0) > 0
                text: KI18n.i18n("%1 window(s) were excluded because they are not supported for capture.",
                                 workspaceService.preview.excludedWindowCount || 0)
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (workspaceService.preview.geometryOmittedCount || 0) > 0
                text: KI18n.i18n("Normal geometry was unavailable for %1 window(s). Their geometry will not be restored from this profile.",
                                 workspaceService.preview.geometryOmittedCount || 0)
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (workspaceService.preview.stateGeometryUnavailableCount || 0) > 0
                text: KI18n.i18n("%1 maximized or fullscreen window(s) do not have a captured normal restore rectangle.",
                                 workspaceService.preview.stateGeometryUnavailableCount || 0)
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (workspaceService.preview.applicationsWithoutLaunchCount || 0) > 0
                text: KI18n.i18n("%1 application(s) have no launch specification. They are recorded for reference; no launch commands have been inferred.",
                                 workspaceService.preview.applicationsWithoutLaunchCount || 0)
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Captured applications")
                font.bold: true
            }

            Repeater {
                model: workspaceService.preview.applications || []

                delegate: Controls.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18n("%1 · %2 window(s)%3",
                                     modelData.label,
                                     modelData.windowCount,
                                     modelData.launchAvailable
                                         ? ""
                                         : KI18n.i18n(" · launch information unavailable"))
                    wrapMode: Text.WordWrap
                }
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (workspaceService.preview.windows || []).length > 0
                text: KI18n.i18n("Captured windows")
                font.bold: true
            }

            Repeater {
                model: workspaceService.preview.windows || []

                delegate: Controls.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18n("%1 · %2 · %3 · %4",
                                     modelData.identity,
                                     (modelData.desktops || []).join(", "),
                                     modelData.geometryAvailable
                                         ? KI18n.i18n("geometry captured")
                                         : KI18n.i18n("geometry unavailable"),
                                     describeWindowState(modelData))
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Controls.Dialog {
        id: profileDetailsDialog
        modal: true
        title: profileInfo.name || KI18n.i18n("Profile Preview")
        standardButtons: Controls.Dialog.Close
        property var profileInfo: ({})

        contentItem: ColumnLayout {
            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("%1 desktops · %2 applications · %3 windows",
                                 profileDetailsDialog.profileInfo.desktopCount || 0,
                                 profileDetailsDialog.profileInfo.applicationCount || 0,
                                 profileDetailsDialog.profileInfo.windowCount || 0)
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Desktops: %1",
                                 (profileDetailsDialog.profileInfo.desktopNames || []).join(", "))
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (profileDetailsDialog.profileInfo.activeDesktopName || "").length > 0
                text: KI18n.i18n("Active desktop: %1", profileDetailsDialog.profileInfo.activeDesktopName || "")
                wrapMode: Text.WordWrap
            }

            Controls.Label {
                Layout.fillWidth: true
                text: KI18n.i18n("Applications")
                font.bold: true
            }

            Repeater {
                model: profileDetailsDialog.profileInfo.applications || []

                delegate: Controls.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18n("%1 · %2 window(s)%3",
                                     modelData.label,
                                     modelData.windowCount,
                                     modelData.launchAvailable
                                         ? ""
                                         : KI18n.i18n(" · launch information unavailable"))
                    wrapMode: Text.WordWrap
                }
            }

            Controls.Label {
                Layout.fillWidth: true
                visible: (profileDetailsDialog.profileInfo.windows || []).length > 0
                text: KI18n.i18n("Window details")
                font.bold: true
            }

            Repeater {
                model: profileDetailsDialog.profileInfo.windows || []

                delegate: Controls.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18n("%1 · %2 · %3 · %4",
                                     modelData.identity,
                                     (modelData.desktops || []).join(", "),
                                     modelData.geometryAvailable
                                         ? KI18n.i18n("geometry captured")
                                         : KI18n.i18n("geometry unavailable"),
                                     describeWindowState(modelData))
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Controls.Dialog {
        id: renameDialog
        modal: true
        title: KI18n.i18n("Rename Profile")
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        property string profileId: ""

        onAccepted: workspaceService.renameProfile(profileId, renameField.text)

        contentItem: Controls.TextField {
            id: renameField
            placeholderText: KI18n.i18n("Profile name")
            onAccepted: renameDialog.accept()
        }
    }

    Controls.Dialog {
        id: deleteDialog
        modal: true
        title: KI18n.i18n("Delete Profile")
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        property string profileId: ""
        property string profileName: ""

        onAccepted: workspaceService.deleteProfile(profileId)

        contentItem: Controls.Label {
            text: KI18n.i18n("Delete “%1”? This cannot be undone.", deleteDialog.profileName)
            wrapMode: Text.WordWrap
        }
    }

    Connections {
        target: workspaceService

        function onPreviewReady() {
            capturePreviewDialog.open()
        }
    }
}
