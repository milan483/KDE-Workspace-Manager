import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.ki18n

Kirigami.ScrollablePage {
    title: KI18n.i18n("Workspaces")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.largeSpacing * 4

        icon.name: "view-grid"
        text: KI18n.i18n("No saved workspaces yet")
        explanation: KI18n.i18n("Workspace profiles will appear here when profile management is available.")
    }
}
