import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

Rectangle {
    id: root

    property var accounts: []
    property string selectedAccountID: ""
    property bool isSwitching: false

    signal selectAccount(string accountID)

    implicitWidth: 276
    implicitHeight: accounts.length > 1 ? bodyLayout.implicitHeight + 4 : 0
    height: implicitHeight
    radius: AppTheme.radiusMd
    color: AppTheme.bgSecondary
    visible: accounts.length > 1
    clip: false

    property bool expanded: false
    property var activeAccount: {
        for (var i = 0; i < accounts.length; i++) {
            if (accounts[i].isActive) return accounts[i]
        }
        return accounts.length > 0 ? accounts[0] : null
    }

    function activeAccountName() {
        if (!activeAccount) return ""
        return activeAccount.displayName || activeAccount.email || "Account"
    }

    function activeAccountInitial() {
        if (!activeAccount) return "A"
        if (activeAccount.isLive) return "S"
        var name = activeAccount.displayName || ""
        var email = activeAccount.email || ""
        return name ? name.charAt(0).toUpperCase() : (email ? email.charAt(0).toUpperCase() : "A")
    }

    ColumnLayout {
        id: bodyLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 4
        spacing: 2

        // Header row
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            radius: 6
            color: headerMouse.containsMouse ? AppTheme.bgHover : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 6

                Text {
                    text: root.expanded ? "▾" : "▸"
                    color: "#888"
                    font.pixelSize: 11
                }

                Rectangle {
                    width: 18
                    height: 18
                    radius: 9
                    color: (activeAccount && activeAccount.isLive) ? "#4CAF50" : "#6b6bff"

                    Text {
                        anchors.centerIn: parent
                        text: root.activeAccountInitial()
                        color: "#ffffff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.activeAccountName()
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fontSizeSm
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    text: accounts.length + " " + qsTr("accounts")
                    color: "#666"
                    font.pixelSize: 10
                }
            }

            MouseArea {
                id: headerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }

            Behavior on color {
                ColorAnimation { duration: 120 }
            }
        }

        // Body - 仅在展开时显示
        Row {
            Layout.fillWidth: true
            Layout.preferredHeight: root.expanded ? 34 : 0
            visible: root.expanded
            spacing: 6
            Layout.leftMargin: 4

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
            }

            Repeater {
                model: root.accounts

                Item {
                    width: 32
                    height: 32

                    property bool isAccountActive: modelData.isActive === true
                    property bool isAccountLive: modelData.isLive === true

                    Rectangle {
                        id: btn
                        width: 32
                        height: 32
                        radius: 16

                        color: isAccountLive ? "#4CAF50" : "#6b6bff"
                        border.width: isAccountActive ? 2 : 0
                        border.color: "#ffffff"

                        Text {
                            anchors.centerIn: parent
                            text: {
                                if (isAccountLive) return "S"
                                var name = modelData.displayName || ""
                                var email = modelData.email || ""
                                return name ? name.charAt(0).toUpperCase() :
                                            (email ? email.charAt(0).toUpperCase() : "A")
                            }
                            color: "#ffffff"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            enabled: !root.isSwitching
                            onClicked: {
                                if (!isAccountActive) {
                                    root.selectAccount(modelData.id)
                                }
                            }
                        }

                        Behavior on color {
                            ColorAnimation { duration: 120; easing.type: Easing.OutQuad }
                        }
                    }

                    // Tooltip - 绝对定位在 root 坐标系中
                    Rectangle {
                        id: tooltip
                        property point globalPos: mapToItem(root, 0, -height - 6)
                        visible: mouseArea.containsMouse
                        x: 16 - width / 2
                        y: -height - 6
                        width: tooltipText.implicitWidth + 12
                        height: tooltipText.implicitHeight + 8
                        radius: 4
                        color: "#1a1a2e"
                        border.color: "#2a2a4a"
                        border.width: 1
                        z: 9999

                        Text {
                            id: tooltipText
                            anchors.centerIn: parent
                            text: {
                                var parts = []
                                var name = modelData.displayName || ""
                                var email = modelData.email || ""
                                if (name) parts.push(name)
                                if (email && email !== name) parts.push(email)
                                if (isAccountLive) parts.push("(" + qsTr("System") + ")")
                                return parts.length > 0 ? parts.join("\n") : "Account"
                            }
                            color: "#ffffff"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: AppTheme.bgPrimary
        opacity: root.isSwitching ? 0.4 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }
}
