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

    Row {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.accounts

            Rectangle {
                id: btn
                width: Math.max(44, Math.floor((root.width - 12 - Math.max(0, root.accounts.length - 1) * 4) / root.accounts.length))
                height: 26
                radius: 6

                property bool isSelected: modelData.id === root.selectedAccountID
                property string displayText: root.compactTitle(modelData, width)

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
                    text: btn.displayText
                    font.pixelSize: AppTheme.fontSizeSm
                    color: btn.isSelected ? "#ffffff" : AppTheme.textSecondary
                    elide: Text.ElideRight
                    maximumLineCount: 1
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

                ToolTip {
                    visible: btnHover.hovered && modelData.displayName !== btn.displayText
                    text: modelData.displayName || modelData.email || ""
                    delay: 500
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

    TextMetrics {
        id: textMetrics
        font.pixelSize: AppTheme.fontSizeSm
    }

    function textWidth(text) {
        textMetrics.text = text
        return textMetrics.advanceWidth
    }

    function truncateTail(text, maxWidth) {
        const trimmed = text.trim()
        if (textWidth(trimmed) <= maxWidth) return trimmed

        const ellipsis = "..."
        const ellipsisWidth = textWidth(ellipsis)
        if (ellipsisWidth >= maxWidth) return ellipsis

        let candidate = ""
        for (let i = 0; i < trimmed.length; i++) {
            const next = candidate + trimmed[i]
            if (textWidth(next + ellipsis) > maxWidth) break
            candidate = next
        }
        return candidate + ellipsis
    }

    function compactTitle(account, buttonWidth) {
        if (!account) return ""

        const horizontalPadding = 14
        const availableTextWidth = Math.max(24, buttonWidth - horizontalPadding)

        const displayName = account.displayName || account.email || ""
        if (textWidth(displayName) <= availableTextWidth) {
            return displayName
        }

        const workspace = account.workspaceLabel || ""
        if (!workspace || workspace === "") {
            return truncateTail(account.email || displayName, availableTextWidth)
        }

        const separator = " | "
        const separatorWidth = textWidth(separator)
        const contentWidth = Math.max(24, availableTextWidth - separatorWidth)

        const minEmailWidth = Math.min(contentWidth * 0.45, Math.max(18, contentWidth * 0.3))
        const minWorkspaceWidth = Math.min(contentWidth * 0.4, Math.max(18, contentWidth * 0.25))

        let emailWidth = Math.max(minEmailWidth, contentWidth * 0.58)
        let workspaceWidth = Math.max(minWorkspaceWidth, contentWidth - emailWidth)

        let title = ""
        let attempts = 0
        do {
            const emailText = truncateTail(account.email || "", emailWidth)
            const workspaceText = truncateTail(workspace, workspaceWidth)

            title = emailText + separator + workspaceText

            const emailRenderedWidth = textWidth(emailText)
            const workspaceRenderedWidth = textWidth(workspaceText)

            if (emailRenderedWidth >= workspaceRenderedWidth && emailWidth > minEmailWidth) {
                emailWidth = Math.max(minEmailWidth, emailWidth - 6)
            } else if (workspaceWidth > minWorkspaceWidth) {
                workspaceWidth = Math.max(minWorkspaceWidth, workspaceWidth - 6)
            } else {
                break
            }
            attempts++
        } while (textWidth(title) > availableTextWidth && attempts < 16)

        return title || truncateTail(account.email || displayName, availableTextWidth)
    }
}
