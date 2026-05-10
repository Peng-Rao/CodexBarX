import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string primaryText: ""
    property string secondaryText: ""
    property string tertiaryText: ""
    property bool floating: false
    property color accentColor: "#4A90D9"

    readonly property bool hasContent: primaryText !== ""

    visible: hasContent
    color: floating ? "#25253e" : "transparent"
    border.color: floating ? "#3b3b5d" : "transparent"
    border.width: floating ? 1 : 0
    radius: floating ? 6 : 0
    implicitWidth: floating ? Math.min(220, Math.max(132, detailLayout.implicitWidth + 18)) : detailLayout.implicitWidth
    implicitHeight: detailLayout.implicitHeight + (floating ? 12 : 0)
    Layout.fillWidth: !floating
    z: floating ? 20 : 0

    Rectangle {
        width: 2
        radius: 1
        color: root.accentColor
        opacity: 0.8
        visible: root.floating
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 7
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
    }

    ColumnLayout {
        id: detailLayout
        anchors.fill: parent
        anchors.leftMargin: root.floating ? 13 : 0
        anchors.rightMargin: root.floating ? 7 : 0
        anchors.topMargin: root.floating ? 6 : 0
        anchors.bottomMargin: root.floating ? 6 : 0
        spacing: 2

        Text {
            Layout.fillWidth: true
            text: root.primaryText
            color: "#ddd"
            font.pixelSize: 11
            font.bold: root.floating
            elide: Text.ElideRight
            maximumLineCount: 1
        }
        Text {
            Layout.fillWidth: true
            text: root.secondaryText
            color: "#aaa"
            font.pixelSize: 10
            visible: root.secondaryText !== ""
            elide: Text.ElideRight
            maximumLineCount: 1
        }
        Text {
            Layout.fillWidth: true
            text: root.tertiaryText
            color: "#888"
            font.pixelSize: 9
            visible: root.tertiaryText !== ""
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
