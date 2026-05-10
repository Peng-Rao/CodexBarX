import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

ColumnLayout {
    id: root

    property string currentProviderID: ""
    property var currentSnapshot: ({})
    property string currentError: ""
    property string dashboardURL: ""

    signal actionTriggered(int actionType, var payload)

    readonly property int actionRefresh: 0
    readonly property int actionDashboard: 1
    readonly property int actionStatusPage: 2
    readonly property int actionCopyError: 3

    width: parent ? parent.width : 276
    spacing: 4

    // Group 1: Provider context
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        spacing: 4
        visible: {
            var url = ""
            if (currentSnapshot.statusURL !== undefined && currentSnapshot.statusURL)
                url = currentSnapshot.statusURL
            return (root.dashboardURL !== "") || (url !== "") || (root.currentError !== "")
        }

        TrayMenuButton {
            text: qsTr("Dashboard")
            visible: root.dashboardURL !== ""
            onClicked: AppController.openExternalUrl(root.dashboardURL)
        }

        TrayMenuButton {
            text: qsTr("Status")
            visible: {
                if (currentSnapshot.statusURL !== undefined && currentSnapshot.statusURL)
                    return true
                return false
            }
            onClicked: {
                var url = currentSnapshot.statusURL || ""
                if (url) AppController.openExternalUrl(url)
            }
        }

        TrayMenuButton {
            text: qsTr("Copy Error")
            visible: root.currentError !== ""
            textColor: "#e06060"
            hoverColor: "#4a3030"
            onClicked: AppController.copyWithFeedback(root.currentError)
        }
    }

    // Separator
    Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        Layout.preferredHeight: 1
        color: AppTheme.borderColor
        visible: {
            var url = ""
            if (currentSnapshot.statusURL !== undefined && currentSnapshot.statusURL)
                url = currentSnapshot.statusURL
            return (root.dashboardURL !== "") || (url !== "") || (root.currentError !== "")
        }
    }

    // Group 2: Tools
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        spacing: 4
        visible: root.currentProviderID === "kilo" || root.currentProviderID === "ollama"

        TrayMenuButton {
            text: qsTr("Terminal")
            visible: root.currentProviderID === "kilo" || root.currentProviderID === "ollama"
            onClicked: {
                var cmd = root.currentProviderID === "kilo" ? "kilo" : "ollama"
                AppController.openTerminal(cmd)
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        Layout.preferredHeight: 1
        color: AppTheme.borderColor
        visible: root.currentProviderID === "kilo" || root.currentProviderID === "ollama"
    }

    // Component: menu button
    component TrayMenuButton: Rectangle {
        property string text: ""
        property color textColor: AppTheme.textSecondary
        property color hoverColor: AppTheme.bgHover

        signal clicked()

        Layout.preferredWidth: btnText.implicitWidth + 20
        Layout.preferredHeight: 26
        radius: 6
        color: btnMouse.hovered ? hoverColor : "transparent"

        Behavior on color { ColorAnimation { duration: 80 } }

        Text {
            id: btnText
            anchors.centerIn: parent
            text: parent.text
            color: parent.textColor
            font.pixelSize: AppTheme.fontSizeSm
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
