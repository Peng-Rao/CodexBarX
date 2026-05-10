import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

Rectangle {
    id: root

    property string title: qsTr("Confirm Delete")
    property string description: ""
    property string itemLabel: ""
    property string itemSublabel: ""
    property bool destructive: true

    signal confirmed()
    signal cancelled()

    anchors.fill: parent
    color: "transparent"
    z: 9999

    function open() {
        visible = true
        overlay.opacity = 0
        dialog.opacity = 0
        dialog.scale = 0.9
        showAnim.start()
    }

    function close() {
        hideAnim.start()
    }

    visible: false

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "#000000"
        opacity: 0

        Behavior on opacity {
            NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
        }

        MouseArea { anchors.fill: parent }
    }

    Rectangle {
        id: dialog
        width: 300
        height: dialogContent.implicitHeight + 32
        anchors.centerIn: parent
        radius: AppTheme.radiusLg
        color: AppTheme.bgPrimary
        border.color: AppTheme.borderColor
        border.width: 1
        opacity: 0

        property real scale: 0.9
        transform: Scale {
            origin.x: dialog.width / 2; origin.y: dialog.height / 2
            xScale: dialog.scale; yScale: dialog.scale
        }

        Behavior on scale {
            NumberAnimation { duration: 200; easing.type: Easing.OutBack }
        }
        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            id: dialogContent
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 16
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: root.title
                color: AppTheme.textPrimary
                font.pixelSize: 14
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                visible: root.description !== ""
                text: root.description
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fontSizeSm
                wrapMode: Text.WordWrap
                maximumLineCount: 3
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: previewText.implicitHeight + 16
                visible: root.itemLabel !== ""
                radius: 6
                color: AppTheme.bgSecondary
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2
                    Text {
                        id: previewText
                        text: root.itemLabel
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fontSizeSm
                    }
                    Text {
                        visible: root.itemSublabel !== ""
                        text: root.itemSublabel
                        color: AppTheme.textTertiary
                        font.pixelSize: 10
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                visible: root.destructive
                text: qsTr("This action cannot be undone.")
                color: "#e06060"
                font.pixelSize: AppTheme.fontSizeSm
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Rectangle {
                    Layout.preferredWidth: cancelBtnText.implicitWidth + 24
                    Layout.preferredHeight: 28
                    radius: 6
                    color: cancelMouse.containsMouse ? AppTheme.bgHover : "transparent"
                    border.color: AppTheme.borderColor
                    border.width: 1
                    Text {
                        id: cancelBtnText
                        anchors.centerIn: parent
                        text: qsTr("Cancel")
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fontSizeSm
                    }
                    MouseArea {
                        id: cancelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.cancelled()
                    }
                    Behavior on color { ColorAnimation { duration: 80 } }
                }
                Rectangle {
                    Layout.preferredWidth: confirmBtnText.implicitWidth + 24
                    Layout.preferredHeight: 28
                    radius: 6
                    color: confirmMouse.containsMouse ? "#5a4040" : "#4a3030"
                    Text {
                        id: confirmBtnText
                        anchors.centerIn: parent
                        text: qsTr("Delete")
                        color: "#e06060"
                        font.pixelSize: AppTheme.fontSizeSm
                    }
                    MouseArea {
                        id: confirmMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.confirmed()
                    }
                    Behavior on color { ColorAnimation { duration: 80 } }
                }
            }
        }
    }

    ParallelAnimation {
        id: showAnim
        NumberAnimation { target: overlay; property: "opacity"; to: 0.5; duration: 150; easing.type: Easing.OutQuad }
        NumberAnimation { target: dialog; property: "opacity"; to: 1; duration: 200; easing.type: Easing.OutBack }
        NumberAnimation { target: dialog; property: "scale"; to: 1.0; duration: 200; easing.type: Easing.OutBack }
    }

    SequentialAnimation {
        id: hideAnim
        NumberAnimation { target: dialog; property: "opacity"; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { target: dialog; property: "scale"; to: 0.95; duration: 150; easing.type: Easing.InCubic }
        PropertyAction { target: root; property: "visible"; value: false }
    }

    focus: visible
    Keys.onEscapePressed: root.cancelled()
    onVisibleChanged: { if (visible) forceActiveFocus() }
}
