import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.ki18n

Kirigami.ScrollablePage {
    title: KI18n.i18n("Settings")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.largeSpacing * 4

        icon.name: "settings-configure"
        text: KI18n.i18n("Settings are not available yet")
        explanation: KI18n.i18n("Application settings will be introduced in a later development unit.")
    }
}
