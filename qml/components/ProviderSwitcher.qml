import QtQuick 2.15

Rectangle {
    id: root
    property var providerList: []
    property string selectedProviderID: ""
    property bool isSwitching: false

    signal selectProvider(string providerId)

    width: parent ? parent.width - 24 : 276
    radius: AppTheme.radiusMd
    color: AppTheme.bgSecondary
    visible: providerList.length > 0
    implicitHeight: 44
    height: implicitHeight

    property var brandColors: ({
        "codex": "#49A3B0", "claude": "#CC7C5E", "cursor": "#5B8DFA",
        "gemini": "#8860D0", "copilot": "#2DA44E", "zai": "#E85A6A",
        "opencode": "#E44D26", "warp": "#00BCD4", "mistral": "#F77F00",
        "openrouter": "#FF6B6B", "ollama": "#E6EF6C", "kilo": "#7C3AED",
        "kiro": "#F59E0B", "kimik2": "#06B6D4", "minimax": "#EC4899",
        "perplexity": "#22C55E", "kimi": "#8B5CF6", "abacus": "#6366F1",
        "alibaba": "#F97316", "augment": "#14B8A6", "amp": "#D946EF",
        "factory": "#84CC16", "jetbrains": "#F000F0", "vertexai": "#4285F4",
        "deepseek": "#4D6BFE", "codebuff": "#44FF00", "windsurf": "#34E8BB",
        "antigravity": "#10B981", "synthetic": "#6366F1", "opencodego": "#3B82F6"
    })

    function brandColorFor(providerId) {
        return brandColors[providerId] || "#4A90D9"
    }

    Flickable {
        id: flicker
        anchors.fill: parent
        anchors.margins: 5
        contentWidth: buttonRow.width
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: {
                if (wheel.angleDelta.y !== 0) {
                    var newX = flicker.contentX - wheel.angleDelta.y;
                    newX = Math.max(0, Math.min(newX, flicker.contentWidth - flicker.width));
                    flicker.contentX = newX;
                    wheel.accepted = true;
                }
            }
        }

        Row {
            id: buttonRow
            spacing: 1
            height: parent.height

            Repeater {
                model: root.providerList
                delegate: Rectangle {
                    id: btn
                    width: 72
                    height: 34
                    radius: 6

                    property bool isSelected: modelData.providerId === root.selectedProviderID

                    color: {
                        if (isSelected) return AppTheme.accentColor;
                        if (btnHover.hovered) return AppTheme.bgHover;
                        return "transparent";
                    }

                    Behavior on color {
                        ColorAnimation { duration: 120; easing.type: Easing.OutQuad }
                    }

                    Column {
                        id: contentRow
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -2
                        spacing: 2

                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 14
                            height: 14
                            source: modelData.iconSource || ""
                            fillMode: Image.PreserveAspectFit
                            visible: !!modelData.iconSource && modelData.iconSource !== ""

                            Rectangle {
                                anchors.fill: parent
                                color: AppTheme.bgTertiary
                                radius: 3
                                visible: parent.status !== Image.Ready && !!modelData.iconSource
                                Text {
                                    anchors.centerIn: parent
                                    text: (modelData.displayName || modelData.providerId || "").charAt(0).toUpperCase()
                                    font.pixelSize: 8
                                    color: "white"
                                }
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.displayName || modelData.providerId
                            font.pixelSize: 10
                            color: btn.isSelected ? "#ffffff" : AppTheme.textSecondary
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        height: 3
                        radius: 1.5
                        color: AppTheme.borderColor
                        visible: !btn.isSelected && modelData.hasUsage === true && modelData.weeklyRemaining !== undefined && modelData.weeklyRemaining !== null

                        Rectangle {
                            width: parent.width * Math.max(0, Math.min(1, (modelData.weeklyRemaining || 0) / 100))
                            height: parent.height
                            radius: 1.5
                            color: root.brandColorFor(modelData.providerId)
                            Behavior on width {
                                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                            }
                        }
                    }

                    TapHandler {
                        enabled: !root.isSwitching
                        onTapped: {
                            if (modelData.providerId !== root.selectedProviderID) {
                                root.selectProvider(modelData.providerId);
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
    }

    // Left scroll hint
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 10
        radius: parent.radius
        visible: flicker.contentWidth > flicker.width && flicker.contentX > 0
        gradient: Gradient {
            GradientStop { position: 0.0; color: AppTheme.bgSecondary }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Right scroll hint
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 10
        radius: parent.radius
        visible: flicker.contentWidth > flicker.width && flicker.contentX < flicker.contentWidth - flicker.width
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: AppTheme.bgSecondary }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: parent.radius
        color: AppTheme.bgPrimary
        opacity: root.isSwitching ? 0.3 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }
}
