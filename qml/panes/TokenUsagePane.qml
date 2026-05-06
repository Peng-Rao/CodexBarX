import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import CodexBar 1.0

Rectangle {
    id: root
    color: AppTheme.bgPrimary

    property var costData: UsageDetailsViewModel.costData
    property var providerRows: UsageDetailsViewModel.providerRows
    property int rev: LanguageManager.translationRevision

    Component.onCompleted: UsageDetailsViewModel.activate()
    Component.onDestruction: UsageDetailsViewModel.deactivate()

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: 18
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Token Usage Overview")
                        color: AppTheme.textPrimary
                        font.pixelSize: 22
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Provider and model breakdown")
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fontSizeSm
                        elide: Text.ElideRight
                    }
                }

                StatusPill {
                    text: !UsageDetailsViewModel.costUsageEnabled
                        ? qsTr("Off")
                        : UsageDetailsViewModel.costUsageRefreshing ? qsTr("Scanning") : qsTr("Last 30 days")
                    toneColor: !UsageDetailsViewModel.costUsageEnabled
                        ? AppTheme.textTertiary
                        : UsageDetailsViewModel.costUsageRefreshing ? AppTheme.statusDegraded : AppTheme.statusOk
                }

                SmallButton {
                    text: UsageDetailsViewModel.costUsageRefreshing ? qsTr("Scanning") : qsTr("Refresh")
                    enabled: UsageDetailsViewModel.costUsageEnabled && !UsageDetailsViewModel.costUsageRefreshing
                    onClicked: UsageDetailsViewModel.refreshCostUsage()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 166
                radius: AppTheme.radiusMd
                color: AppTheme.bgCard
                border.color: AppTheme.borderColor
                border.width: 1
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 4
                    color: AppTheme.accentColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    anchors.leftMargin: 18
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 62
                        spacing: 12

                        SummaryMetric {
                            title: qsTr("Today")
                            value: "$" + root.formatCost(root.costData.sessionCostUSD || 0)
                            detail: root.fmtNum(root.costData.sessionTokens || 0) + " " + qsTr("tokens")
                            accentColor: AppTheme.accentColor
                            Layout.fillWidth: true
                        }

                        SummaryMetric {
                            title: qsTr("30 days")
                            value: "$" + root.formatCost(root.costData.last30DaysCostUSD || 0)
                            detail: root.fmtNum(root.costData.last30DaysTokens || 0) + " " + qsTr("tokens")
                            accentColor: AppTheme.statusOk
                            Layout.fillWidth: true
                        }

                        SummaryMetric {
                            title: qsTr("Providers")
                            value: UsageDetailsViewModel.tokenProviderCount.toString()
                            detail: qsTr("token sources")
                            accentColor: AppTheme.statusDegraded
                            Layout.fillWidth: true
                        }
                    }

                    MiniBars {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        daily: root.lastItems(root.costData.daily || [], 30)
                        tintColor: AppTheme.accentColor
                    }
                }
            }

            Repeater {
                model: root.providerRows

                delegate: ProviderUsageCard {
                    Layout.fillWidth: true
                    provider: modelData
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: AppTheme.radiusSm
                color: AppTheme.bgSecondary
                border.color: AppTheme.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10

                    Label {
                        text: qsTr("Total")
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fontSizeMd
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: "$" + root.formatCost(root.costData.last30DaysCostUSD || 0)
                            + " · " + root.fmtNum(root.costData.last30DaysTokens || 0) + " " + qsTr("tokens")
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fontSizeMd
                        font.bold: true
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                visible: root.providerRows.length === 0
                spacing: 8

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("No token providers enabled")
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fontSizeMd
                    horizontalAlignment: Text.AlignHCenter
                }

                Item { Layout.fillHeight: true }
            }

            Item { Layout.preferredHeight: 8 }
        }
    }

    function kindLabel(kind) {
        if (kind === "credit") return qsTr("Credit")
        if (kind === "quota") return qsTr("Quota")
        return qsTr("Token")
    }

    function lastItems(items, count) {
        if (!items) return []
        var start = Math.max(0, items.length - count)
        var result = []
        for (var i = start; i < items.length; ++i) result.push(items[i])
        return result
    }

    function dailyValue(day) {
        if (!day) return 0
        var cost = Number(day.costUSD || 0)
        if (cost > 0) return cost
        return Number(day.totalTokens || 0)
    }

    function maxDailyValue(days) {
        var maxValue = 0
        if (!days) return 0
        for (var i = 0; i < days.length; ++i) {
            maxValue = Math.max(maxValue, root.dailyValue(days[i]))
        }
        return maxValue
    }

    function usageSummary(provider, costKey, tokenKey) {
        if (!provider || !provider.hasTokenData) return provider ? root.kindLabel(provider.kind || "token") : ""
        return "$" + root.formatCost(provider[costKey] || 0)
            + " · " + root.fmtNum(provider[tokenKey] || 0) + " " + qsTr("tokens")
    }

    function fmtNum(n) {
        var value = Number(n || 0)
        if (value >= 1000000) return (value / 1000000).toFixed(1) + "M"
        if (value >= 1000) return (value / 1000).toFixed(1) + "K"
        return value.toString()
    }

    function formatCost(n) {
        var value = Number(n || 0)
        var absValue = Math.abs(value)
        if (absValue === 0) return "0.00"
        if (absValue < 0.01) return value.toFixed(4)
        if (absValue >= 1000) return value.toFixed(1)
        return value.toFixed(2)
    }

    function brandColorFor(providerId) {
        var colors = {
            "codex": "#49A3B0",
            "claude": "#CC7C5E",
            "cursor": "#5B8DFA",
            "gemini": "#8860D0",
            "copilot": "#2DA44E",
            "zai": "#E85A6A",
            "opencode": "#E44D26",
            "opencodego": "#3B82F6",
            "warp": "#00BCD4",
            "mistral": "#F77F00",
            "openrouter": "#FF6B6B",
            "ollama": "#E6EF6C",
            "kilo": "#7C3AED",
            "kiro": "#F59E0B",
            "kimik2": "#06B6D4",
            "minimax": "#EC4899",
            "perplexity": "#22C55E",
            "kimi": "#8B5CF6",
            "abacus": "#6366F1",
            "alibaba": "#F97316",
            "augment": "#14B8A6",
            "amp": "#D946EF",
            "factory": "#84CC16",
            "jetbrains": "#F000F0",
            "vertexai": "#4285F4",
            "deepseek": "#4D6BFE",
            "antigravity": "#10B981",
            "synthetic": "#6366F1"
        }
        return colors[providerId] || "#4A90D9"
    }

    component SummaryMetric: Item {
        id: metric
        property string title: ""
        property string value: ""
        property string detail: ""
        property color accentColor: AppTheme.accentColor

        implicitHeight: 58
        clip: true

        Column {
            anchors.fill: parent
            spacing: 3

            Text {
                width: parent.width
                text: metric.title
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fontSizeSm
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: metric.value
                color: metric.accentColor
                font.pixelSize: 21
                minimumPixelSize: 13
                fontSizeMode: Text.HorizontalFit
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: metric.detail
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSizeSm
                elide: Text.ElideRight
            }
        }
    }

    component StatusPill: Rectangle {
        id: pill
        property string text: ""
        property color toneColor: AppTheme.statusUnknown

        readonly property int desiredWidth: Math.max(72, pillLabel.implicitWidth + 20)

        Layout.preferredWidth: desiredWidth
        Layout.minimumWidth: desiredWidth
        Layout.maximumWidth: desiredWidth
        Layout.preferredHeight: 26
        radius: 13
        color: "transparent"
        border.width: 1
        border.color: toneColor
        opacity: enabled ? 1 : 0.6

        Label {
            id: pillLabel
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            text: pill.text
            color: pill.toneColor
            font.pixelSize: AppTheme.fontSizeSm
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component SmallButton: Rectangle {
        id: button
        property string text: ""
        signal clicked()

        readonly property int desiredWidth: Math.max(72, buttonLabel.implicitWidth + 22)

        Layout.preferredWidth: desiredWidth
        Layout.minimumWidth: desiredWidth
        Layout.maximumWidth: desiredWidth
        Layout.preferredHeight: 30
        radius: AppTheme.radiusSm
        color: !enabled ? AppTheme.bgSecondary : (buttonMouse.containsMouse ? AppTheme.bgHover : AppTheme.bgCard)
        border.width: 1
        border.color: enabled ? AppTheme.borderAccent : AppTheme.borderColor
        opacity: enabled ? 1 : 0.58

        Label {
            id: buttonLabel
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            text: button.text
            color: button.enabled ? AppTheme.textPrimary : AppTheme.textTertiary
            font.pixelSize: AppTheme.fontSizeSm
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: {
                if (button.enabled) button.clicked()
            }
        }
    }

    component MiniBars: Item {
        id: chart
        property var daily: []
        property color tintColor: AppTheme.accentColor
        property real maxValue: root.maxDailyValue(daily)

        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: AppTheme.borderColor
        }

        Row {
            id: bars
            anchors.fill: parent
            spacing: 2

            Repeater {
                model: chart.daily

                delegate: Item {
                    width: chart.daily.length > 0
                        ? Math.max(3, (bars.width - Math.max(0, chart.daily.length - 1) * bars.spacing) / chart.daily.length)
                        : bars.width
                    height: bars.height

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: chart.maxValue > 0
                            ? Math.max(2, parent.height * root.dailyValue(modelData) / chart.maxValue)
                            : 2
                        radius: 2
                        color: chart.tintColor
                        opacity: 0.78
                    }
                }
            }
        }
    }

    component ProviderUsageCard: Rectangle {
        id: card
        property var provider: ({})
        property bool expanded: provider.hasTokenData && ((provider.models || []).length > 0)
        readonly property bool canExpand: provider.hasTokenData
            && (((provider.models || []).length > 0) || ((provider.daily || []).length > 0))
        readonly property color accentColor: provider.brandColor || root.brandColorFor(provider.providerId)

        Layout.preferredHeight: contentColumn.implicitHeight + 24
        radius: AppTheme.radiusMd
        color: AppTheme.bgCard
        border.color: AppTheme.borderColor
        border.width: 1
        opacity: card.provider.enabled === false ? 0.64 : 1
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            color: card.accentColor
        }

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            anchors.leftMargin: 16
            spacing: 10

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30

                RowLayout {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        Layout.preferredWidth: 14
                        text: card.canExpand ? (card.expanded ? "▾" : "▸") : "•"
                        color: card.canExpand ? AppTheme.textSecondary : card.accentColor
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: card.accentColor
                    }

                    Label {
                        Layout.fillWidth: true
                        text: card.provider.displayName || card.provider.providerId
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fontSizeMd
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    StatusPill {
                        text: root.kindLabel(card.provider.kind || "token")
                        toneColor: card.accentColor
                    }

                    Label {
                        Layout.preferredWidth: Math.min(230, Math.max(150, implicitWidth))
                        text: root.usageSummary(card.provider, "last30DaysCostUSD", "last30DaysTokens")
                        color: card.provider.hasTokenData ? AppTheme.textSecondary : AppTheme.textPrimary
                        font.pixelSize: AppTheme.fontSizeSm
                        font.bold: !card.provider.hasTokenData
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: card.canExpand
                    hoverEnabled: true
                    cursorShape: card.canExpand ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: card.expanded = !card.expanded
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                Layout.preferredHeight: card.provider.hasTokenData ? 50 : 4
                spacing: 12
                visible: card.provider.hasTokenData

                StatBlock {
                    title: qsTr("Today")
                    value: "$" + root.formatCost(card.provider.sessionCostUSD || 0)
                    detail: root.fmtNum(card.provider.sessionTokens || 0) + " " + qsTr("tokens")
                    Layout.fillWidth: true
                }

                StatBlock {
                    title: qsTr("30 days")
                    value: "$" + root.formatCost(card.provider.last30DaysCostUSD || 0)
                    detail: root.fmtNum(card.provider.last30DaysTokens || 0) + " " + qsTr("tokens")
                    Layout.fillWidth: true
                }

                MiniBars {
                    Layout.preferredWidth: 180
                    Layout.maximumWidth: 180
                    Layout.fillHeight: true
                    daily: root.lastItems(card.provider.daily || [], 30)
                    tintColor: card.accentColor
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 22
                visible: card.expanded && card.canExpand
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: AppTheme.borderColor
                }

                Repeater {
                    model: card.provider.models || []

                    delegate: RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            text: modelData.name || qsTr("Unknown model")
                            color: AppTheme.textSecondary
                            font.pixelSize: AppTheme.fontSizeSm
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.preferredWidth: 92
                            text: "$" + root.formatCost(modelData.costUSD || 0)
                            color: AppTheme.textSecondary
                            font.pixelSize: AppTheme.fontSizeSm
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.preferredWidth: 100
                            text: root.fmtNum(modelData.tokens || 0) + " " + qsTr("tokens")
                            color: AppTheme.textTertiary
                            font.pixelSize: AppTheme.fontSizeSm
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    component StatBlock: Item {
        id: stat
        property string title: ""
        property string value: ""
        property string detail: ""

        implicitHeight: 46
        clip: true

        Column {
            anchors.fill: parent
            spacing: 2

            Text {
                width: parent.width
                text: stat.title
                color: AppTheme.textTertiary
                font.pixelSize: AppTheme.fontSizeSm
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: stat.value
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fontSizeMd
                font.bold: true
                minimumPixelSize: 10
                fontSizeMode: Text.HorizontalFit
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: stat.detail
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fontSizeSm
                elide: Text.ElideRight
            }
        }
    }
}
