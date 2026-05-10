import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

Rectangle {
    id: root

    property var accounts: []
    property string selectedAccountID: ""
    property bool isSwitching: false

    signal selectAccount(string accountID)

    width: parent ? parent.width - 24 : 276
    implicitHeight: accounts.length > 1 ? 34 : 0
    height: implicitHeight
    radius: AppTheme.radiusMd
    color: AppTheme.bgSecondary
    visible: accounts.length > 1

    property int perRow: accounts.length <= 3 ? accounts.length : Math.ceil(accounts.length / 2)

    Row {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.accounts

            Rectangle {
                id: btn
                width: Math.max(44, Math.floor((root.width - 8 - Math.max(0, root.perRow - 1) * 4) / root.perRow))
                height: 26
                radius: 6

                property bool isSelected: modelData.id === root.selectedAccountID

                color: {
                    if (isSelected) return AppTheme.accentColor
                    if (btnHover.hovered) return AppTheme.bgHover
                    return "transparent"
                }

                Behavior on color {
                    ColorAnimation { duration: 120; easing.type: Easing.OutQuad }
                }

                Text {
                    anchors.centerIn: parent
                    text: modelData.displayName || modelData.email || "Account"
                    font.pixelSize: AppTheme.fontSizeSm
                    color: btn.isSelected ? "#ffffff" : AppTheme.textSecondary
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    width: parent.width - 8
                    horizontalAlignment: Text.AlignHCenter
                }

                TapHandler {
                    enabled: !root.isSwitching
                    onTapped: {
                        if (modelData.id !== root.selectedAccountID) {
                            root.selectAccount(modelData.id)
                        }
                    }
                }

                HoverHandler {
                    id: btnHover
                    cursorShape: Qt.PointingHandCursor
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
