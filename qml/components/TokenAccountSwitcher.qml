import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

Rectangle {
    id: root

    property var accounts: []
    property string selectedAccountID: ""
    property bool isSwitching: false

    signal selectAccount(string accountID)

    width: parent ? parent.width : 276
    height: visible ? buttonRow.implicitHeight + 8 : 0
    radius: AppTheme.radiusMd
    color: AppTheme.bgSecondary
    visible: accounts.length > 1

    property int rowCount: accounts.length <= 3 ? 1 : 2
    property int perRow: rowCount === 1 ? accounts.length : Math.ceil(accounts.length / 2)
    property real buttonWidth: {
        const gaps = Math.max(0, perRow - 1) * 4
        return Math.max(44, Math.floor((width - 8 - gaps) / perRow))
    }

    Row {
        id: buttonRow
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.accounts

            Rectangle {
                id: btn
                width: root.buttonWidth
                height: 26
                radius: 6

                property bool isSelected: modelData.accountId === root.selectedAccountID

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
                    text: modelData.displayName || modelData.accountId || "Account"
                    font.pixelSize: AppTheme.fontSizeSm
                    color: btn.isSelected ? "#ffffff" : AppTheme.textSecondary
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                TapHandler {
                    enabled: !root.isSwitching
                    onTapped: {
                        if (modelData.accountId !== root.selectedAccountID) {
                            root.selectAccount(modelData.accountId)
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
