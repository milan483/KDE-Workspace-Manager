import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.ki18n

Kirigami.ApplicationWindow {
    id: root

    objectName: "mainWindow"

    width: 960
    height: 640
    minimumWidth: 640
    minimumHeight: 480

    title: KI18n.i18n("KDE Workspace Manager")

    pageStack.objectName: "mainPageStack"
    pageStack.initialPage: WorkspacesPage {}

    globalDrawer: Kirigami.GlobalDrawer {
        title: KI18n.i18n("KDE Workspace Manager")

        actions: [
            Kirigami.Action {
                objectName: "workspacesNavigationAction"
                text: KI18n.i18n("Workspaces")
                icon.name: "view-grid"
                onTriggered: root.showWorkspaces()
            },
            Kirigami.Action {
                objectName: "settingsNavigationAction"
                text: KI18n.i18n("Settings")
                icon.name: "settings-configure"
                onTriggered: root.showSettings()
            }
        ]
    }

    Component {
        id: workspacesPageComponent

        WorkspacesPage {}
    }

    Component {
        id: settingsPageComponent

        SettingsPage {}
    }

    function showWorkspaces(): void {
        pageStack.replace(workspacesPageComponent)
    }

    function showSettings(): void {
        pageStack.replace(settingsPageComponent)
    }
}
