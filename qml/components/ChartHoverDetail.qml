import QtQuick 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: root

    property string primaryText: ""
    property string secondaryText: ""
    property string tertiaryText: ""

    readonly property bool hasContent: primaryText !== ""

    visible: hasContent
    spacing: 2
    Layout.fillWidth: true

    Text {
        Layout.fillWidth: true
        text: root.primaryText
        color: "#aaa"
        font.pixelSize: 11
        elide: Text.ElideRight
        maximumLineCount: 1
    }
    Text {
        Layout.fillWidth: true
        text: root.secondaryText
        color: "#888"
        font.pixelSize: 10
        visible: root.secondaryText !== ""
        elide: Text.ElideRight
        maximumLineCount: 1
    }
    Text {
        Layout.fillWidth: true
        text: root.tertiaryText
        color: "#666"
        font.pixelSize: 9
        visible: root.tertiaryText !== ""
        elide: Text.ElideRight
        maximumLineCount: 1
    }
}
