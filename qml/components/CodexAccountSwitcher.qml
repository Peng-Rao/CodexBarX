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
    implicitHeight: accounts.length > 1 ? 40 : 0
    height: implicitHeight
    radius: AppTheme.radiusMd
    color: AppTheme.bgSecondary
    visible: accounts.length > 1

    Row {
        id: buttonRow
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.accounts

            Item {
                id: btnContainer
                width: 32
                height: 32 + (tooltip.visible ? tooltip.height + 6 : 0)

                property bool isAccountActive: modelData.isActive === true
                property bool isAccountLive: modelData.isLive === true

                Rectangle {
                    id: btn
                    width: 32
                    height: 32
                    radius: 16
                    anchors.top: parent.top

                    // 背景色：系统账户绿色，其他账户紫色
                    color: btnContainer.isAccountLive ? "#4CAF50" : "#6b6bff"

                    // 选中的账户显示白色边框
                    border.width: btnContainer.isAccountActive ? 2 : 0
                    border.color: "#ffffff"

                    // 首字母
                    Text {
                        anchors.centerIn: parent
                        property string name: modelData.displayName || ""
                        property string email: modelData.email || ""
                        text: btnContainer.isAccountLive ? "S" : (name ? name.charAt(0).toUpperCase() :
                                              (email ? email.charAt(0).toUpperCase() : "A"))
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
                            if (!btnContainer.isAccountActive) {
                                root.selectAccount(modelData.id)
                            }
                        }
                    }

                    Behavior on color {
                        ColorAnimation { duration: 120; easing.type: Easing.OutQuad }
                    }
                }

                // Tooltip 显示在上方
                Rectangle {
                    id: tooltip
                    visible: mouseArea.containsMouse
                    x: btn.width / 2 - width / 2
                    y: -height - 6
                    width: tooltipText.implicitWidth + 12
                    height: tooltipText.implicitHeight + 8
                    radius: 4
                    color: "#1a1a2e"
                    border.color: "#2a2a4a"
                    border.width: 1
                    z: 1000

                    Text {
                        id: tooltipText
                        anchors.centerIn: parent
                        property string name: modelData.displayName || ""
                        property string email: modelData.email || ""
                        text: {
                            var parts = []
                            if (name) parts.push(name)
                            if (email && email !== name) parts.push(email)
                            if (btnContainer.isAccountLive) parts.push("(" + qsTr("System") + ")")
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
