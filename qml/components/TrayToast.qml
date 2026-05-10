import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string message: ""
    property int toastType: 0 // 0=info, 1=success, 2=error, 3=warning

    readonly property int typeInfo: 0
    readonly property int typeSuccess: 1
    readonly property int typeError: 2
    readonly property int typeWarning: 3

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 52
    width: Math.min(parent.width - 24, messageText.implicitWidth + 40)
    height: messageText.implicitHeight + 16
    radius: 8
    color: "#252540"
    border.width: 1
    border.color: toastType === typeError ? "#4a3030" : "#2a2a4a"
    z: 100
    opacity: 0
    y: anchors.bottomMargin + 20

    function show(msg, type) {
        message = msg
        toastType = type !== undefined ? type : typeInfo
        hideTimer.stop()
        showAnim.start()
        hideTimer.restart()
    }

    ParallelAnimation {
        id: showAnim
        NumberAnimation { target: root; property: "opacity"; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "y"; to: anchors.bottomMargin; duration: 200; easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        id: hideAnim
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { target: root; property: "y"; to: anchors.bottomMargin + 20; duration: 150; easing.type: Easing.InCubic }
    }

    Timer {
        id: hideTimer
        interval: toastType === typeError ? 4000 : 2500
        onTriggered: hideAnim.start()
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 8

        Text {
            text: {
                switch (root.toastType) {
                    case root.typeSuccess: return "✓"
                    case root.typeError: return "✕"
                    case root.typeWarning: return "⚠"
                    default: return "ℹ"
                }
            }
            color: root.toastType === root.typeError ? "#F44336"
                 : root.toastType === root.typeSuccess ? "#4CAF50"
                 : root.toastType === root.typeWarning ? "#FFC107"
                 : "#aaaaaa"
            font.pixelSize: 14
        }

        Text {
            id: messageText
            text: root.message
            color: root.toastType === root.typeError ? "#F44336" : "#ffffff"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
