import QtQuick 2.15

Rectangle {
    id: root
    property string copyText: ""
    property bool didCopy: false
    property int feedbackDuration: 900

    width: 28
    height: 28
    radius: AppTheme.radiusSm
    color: copyMouse.pressed ? AppTheme.bgPressed
        : (copyMouse.containsMouse ? AppTheme.bgHover : "transparent")

    Behavior on color {
        ColorAnimation { duration: 80; easing.type: Easing.OutQuad }
    }

    Item {
        anchors.centerIn: parent
        width: 14
        height: 14

        Text {
            anchors.centerIn: parent
            text: "⎘"
            font.pixelSize: 12
            color: AppTheme.textSecondary
            opacity: root.didCopy ? 0 : 1
            Behavior on opacity {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
        }

        Text {
            anchors.centerIn: parent
            text: "✓"
            font.pixelSize: 12
            color: AppTheme.statusOk
            opacity: root.didCopy ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
        }
    }

    scale: copyMouse.pressed ? 0.92 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
    }

    MouseArea {
        id: copyMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            AppController.copyWithFeedback(root.copyText);
            root.didCopy = true;
            copyResetTimer.start();
        }
    }

    Timer {
        id: copyResetTimer
        interval: root.feedbackDuration
        onTriggered: root.didCopy = false
    }
}
