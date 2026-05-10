import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0
import ".."

Rectangle {
    id: root

    property string providerId: ""
    property var snap: ({})
    property var tokenAccounts: []
    property string defaultTokenAccountId: ""
    property var accountOptions: []
    property string statusUrl: ""
    property var dashboard: ({})

    property bool isRefreshing: false
    property bool embedded: false

    color: root.embedded ? "transparent" : "#202038"
    implicitHeight: cardContent.implicitHeight + (root.embedded ? 0 : 24)
    radius: root.embedded ? 0 : 10
    border.color: "#2a2a4a"
    border.width: root.embedded ? 0 : 1

    property bool isDetailProvider: providerId === "deepseek"
        || providerId === "warp"
        || providerId === "kilo"
        || providerId === "abacus"
        || providerId === "codebuff"

    property color brandColor: brandColorFor(providerId)
    property bool hasTokenAccounts: tokenAccounts && tokenAccounts.length > 0
    property string primaryLabel: snap.displayName === "OpenRouter" && snap.openRouterUsage !== undefined
        ? qsTr("API key limit") : snap.sessionLabel

    ColumnLayout {
        id: cardContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 6

        // === Title row ===
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                text: snap.displayName || providerId
                color: "white"
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
            }
            Text {
                text: snap.loginMethod && snap.loginMethod !== "" ? snap.loginMethod : ""
                color: "#888"
                font.pixelSize: 10
                visible: !!snap.loginMethod && snap.loginMethod !== ""
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#2a2a4a"
            visible: root.embedded
        }

        // === Token account switcher ===
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.hasTokenAccounts

            Text {
                text: qsTr("Account")
                color: "#888"
                font.pixelSize: 10
                Layout.preferredWidth: 54
            }

            SettingsComboBox {
                objectName: "detailAccountSwitcher_" + root.providerId
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                model: root.accountOptions
                selectedValue: root.defaultTokenAccountId
                onValueActivated: function(value) {
                    if (value !== root.defaultTokenAccountId) {
                        TrayViewModel.requestSetDefaultTokenAccount(root.providerId, value)
                    }
                }
            }
        }

        // === Error row ===
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: snap.error !== undefined && snap.error !== ""
            Text {
                text: snap.error || ""
                color: "#F44336"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
            }
        }

        // === Refresh status ===
        Text {
            Layout.fillWidth: true
            visible: !snap.hasUsage && (snap.error === undefined || snap.error === "")
            text: root.isRefreshing ? qsTr("Refreshing...") : qsTr("No usage yet")
            color: "#555"
            font.pixelSize: 12
        }

        // === Primary bar ===
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true
            spacing: 6
            Text {
                text: root.isDetailProvider ? qsTr("Balance") : root.primaryLabel
                color: "#aaa"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 80
            }
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: "#2a2a4a"
                Rectangle {
                    width: Math.max(0, parent.width * (root.isDetailProvider ? snap.primaryRemaining : snap.primaryUsed) / 100)
                    height: parent.height
                    radius: 3
                    color: root.brandColor
                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    visible: !root.isDetailProvider && snap.primaryPacePercent !== undefined && snap.primaryPacePercent >= 0
                    x: Math.max(0, Math.min(parent.width - 3, parent.width * (snap.primaryPacePercent || 0) / 100 - 1))
                    width: 3
                    height: parent.height
                    color: (snap.primaryPaceOnTop !== false) ? "#4CAF50" : "#F44336"
                    radius: 1
                    Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
            }
            Text {
                text: (snap.primaryDisplayPercent !== undefined ? snap.primaryDisplayPercent : snap.primaryRemaining).toFixed(0) + "%"
                color: snap.primaryRemaining > 50 ? "#4CAF50"
                     : snap.primaryRemaining > 20 ? "#FFC107"
                     : "#F44336"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
            }
        }

        // Primary pace + reset
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.primaryPaceLeftLabel !== undefined
            spacing: 4
            Text {
                text: snap.primaryPaceLeftLabel || ""
                color: "#aaa"
                font.pixelSize: 10
                Layout.preferredWidth: 80
            }
            Text {
                text: snap.primaryPaceRightLabel || ""
                color: "#888"
                font.pixelSize: 10
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.primaryResetDesc !== undefined && snap.primaryResetDesc !== ""
            spacing: 4
            Item { Layout.preferredWidth: 80 }
            Text {
                text: qsTr("Resets") + " " + (snap.primaryResetDesc || "")
                color: "#666"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // === Primary detail ===
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.primaryDetail !== undefined && snap.primaryDetail !== ""
            spacing: 4
            Item { Layout.preferredWidth: 80 }
            Text {
                text: snap.primaryDetail || ""
                color: "#888"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // === Secondary bar ===
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.hasSecondary === true
            spacing: 6
            Text {
                text: snap.weeklyLabel || ""
                color: "#aaa"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 80
            }
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: "#2a2a4a"
                Rectangle {
                    width: Math.max(0, parent.width * snap.secondaryUsed / 100)
                    height: parent.height
                    radius: 3
                    color: root.brandColor
                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    visible: snap.secondaryPacePercent !== undefined && snap.secondaryPacePercent >= 0
                    x: Math.max(0, Math.min(parent.width - 3, parent.width * (snap.secondaryPacePercent || 0) / 100 - 1))
                    width: 3
                    height: parent.height
                    color: (snap.secondaryPaceOnTop !== false) ? "#4CAF50" : "#F44336"
                    radius: 1
                    Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
            }
            Text {
                text: (snap.secondaryDisplayPercent !== undefined ? snap.secondaryDisplayPercent : snap.secondaryRemaining).toFixed(0) + "%"
                color: snap.secondaryRemaining > 50 ? "#2196F3"
                     : snap.secondaryRemaining > 20 ? "#FFC107"
                     : "#F44336"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
            }
        }

        // Secondary pace + reset
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.hasSecondary === true && snap.secondaryPaceLeftLabel !== undefined
            spacing: 4
            Text {
                text: snap.secondaryPaceLeftLabel || ""
                color: "#aaa"
                font.pixelSize: 10
                Layout.preferredWidth: 80
            }
            Text {
                text: snap.secondaryPaceRightLabel || ""
                color: "#888"
                font.pixelSize: 10
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.hasSecondary === true && snap.secondaryResetDesc !== undefined && snap.secondaryResetDesc !== ""
            spacing: 4
            Item { Layout.preferredWidth: 80 }
            Text {
                text: qsTr("Resets") + " " + (snap.secondaryResetDesc || "")
                color: "#666"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // === Tertiary bar ===
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.hasTertiary === true
            spacing: 6
            Text {
                text: snap.opusLabel || qsTr("Opus")
                color: "#aaa"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 80
            }
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: "#2a2a4a"
                Rectangle {
                    width: Math.max(0, parent.width * (snap.tertiaryUsed || 0) / 100)
                    height: parent.height
                    radius: 3
                    color: root.brandColor
                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
            }
            Text {
                text: (snap.tertiaryDisplayPercent !== undefined
                    ? snap.tertiaryDisplayPercent
                    : (snap.tertiaryRemaining !== undefined ? snap.tertiaryRemaining : 100)).toFixed(0) + "%"
                color: snap.tertiaryRemaining > 50 ? "#9C27B0"
                     : snap.tertiaryRemaining > 20 ? "#FFC107"
                     : "#F44336"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 50
                horizontalAlignment: Text.AlignRight
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.hasTertiary === true && snap.tertiaryResetDesc !== undefined && snap.tertiaryResetDesc !== ""
            spacing: 4
            Item { Layout.preferredWidth: 80 }
            Text {
                text: qsTr("Resets") + " " + (snap.tertiaryResetDesc || "")
                color: "#666"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        // === Codex Credits ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            visible: root.providerId === "codex" && snap.hasCredits === true
            color: "#1a2a1a"
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: qsTr("Credits")
                    color: "#888"
                    font.pixelSize: 11
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "$" + (snap.creditsRemaining || 0).toFixed(2) + " left"
                    color: (snap.creditsRemaining || 0) > 10 ? "#4CAF50"
                         : (snap.creditsRemaining || 0) > 2 ? "#FFC107"
                         : "#F44336"
                    font.pixelSize: 13
                    font.bold: true
                }
            }
        }

        // === Codex Credits Error ===
        Text {
            Layout.fillWidth: true
            visible: root.providerId === "codex" && snap.creditsError !== undefined && snap.creditsError !== ""
            text: snap.creditsError || ""
            color: "#FF9800"
            font.pixelSize: 10
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        // === Provider cost bar ===
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasProviderCost === true
            spacing: 6
            Text {
                text: snap.providerCostCurrency === "Quota" ? qsTr("Quota usage") : qsTr("Extra usage")
                color: "#aaa"
                font.pixelSize: 11
                font.bold: true
                Layout.preferredWidth: 80
            }
            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: "#2a2a4a"
                Rectangle {
                    width: Math.max(0, parent.width * Math.min(100, (snap.providerCostUsed || 0) / Math.max(1, snap.providerCostLimit || 1) * 100) / 100)
                    height: parent.height
                    radius: 3
                    color: "#FF9800"
                    Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: snap.hasProviderCost === true
            spacing: 4
            Item { Layout.preferredWidth: 80 }
            Text {
                text: qsTr("This month") + ": $" + (snap.providerCostUsed || 0).toFixed(2) + " / $" + (snap.providerCostLimit || 0).toFixed(2)
                color: "#888"
                font.pixelSize: 10
                Layout.fillWidth: true
            }
            Text {
                text: {
                    var pct = snap.providerCostLimit > 0 ? ((snap.providerCostUsed || 0) / (snap.providerCostLimit || 1) * 100).toFixed(0) : "0"
                    return qsTr("%1 used").arg(pct)
                }
                color: "#666"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignRight
            }
        }

        // === Updated timestamp ===
        Text {
            Layout.fillWidth: true
            visible: snap.hasUsage === true && snap.updatedAt > 0 && !root.isRefreshing
            text: qsTr("Updated") + " " + timeAgo(snap.updatedAt)
            color: "#555"
            font.pixelSize: 9
        }

        // === Expanded detail section ===
        ColumnLayout {
            Layout.fillWidth: true
            visible: snap.hasUsage === true
            spacing: 4
            Layout.topMargin: 4

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#2a2a4a"
            }

            // Zai MCP details
            ColumnLayout {
                Layout.fillWidth: true
                visible: snap.zaiUsage !== undefined && snap.zaiUsage.timeLimit !== undefined
                spacing: 2
                Text {
                    text: qsTr("MCP details")
                    color: "#aaa"
                    font.pixelSize: 10
                    font.bold: true
                }
                Repeater {
                    model: snap.zaiUsage && snap.zaiUsage.timeLimit ? snap.zaiUsage.timeLimit.usageDetails : []
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: modelData.modelCode || ""
                            color: "#888"
                            font.pixelSize: 9
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: fmtNum(modelData.usage)
                            color: "#aaa"
                            font.pixelSize: 9
                        }
                    }
                }
            }

            // OpenRouter key quota status
            ColumnLayout {
                Layout.fillWidth: true
                visible: snap.openRouterUsage !== undefined
                spacing: 2
                Text {
                    visible: !!snap.openRouterUsage && snap.openRouterUsage.keyQuotaStatus === 1
                    text: qsTr("No limit set for the API key")
                    color: "#FFC107"
                    font.pixelSize: 10
                }
                Text {
                    visible: !!snap.openRouterUsage && snap.openRouterUsage.keyQuotaStatus === 2
                    text: qsTr("API key limit unavailable right now")
                    color: "#F44336"
                    font.pixelSize: 10
                }
                Text {
                    visible: !!snap.openRouterUsage && snap.openRouterUsage.keyRemaining !== undefined
                    text: snap.openRouterUsage && snap.openRouterUsage.keyRemaining !== undefined
                        ? "$" + (snap.openRouterUsage.keyRemaining || 0).toFixed(2) + "/$" + (snap.openRouterUsage.keyLimit || 0).toFixed(2) + " " + qsTr("left")
                        : ""
                    color: "#aaa"
                    font.pixelSize: 10
                }
            }

            // Subscription Utilization chart
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: active ? 130 : 0
                active: root.providerId === "codex" || root.providerId === "claude"
                sourceComponent: PlanUtilizationChart {
                    providerId: root.providerId
                    hasTertiarySeries: snap.hasTertiary === true
                    tertiarySeriesLabel: snap.opusLabel || qsTr("Opus")
                    dataRevision: TrayViewModel.providerDataRevision
                }
            }

            // Cost History Chart (Phase B)
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: active ? item.implicitHeight : 0
                active: root.providerId === "codex" || root.providerId === "claude"
                sourceComponent: CostHistoryChart {
                    providerId: root.providerId
                }
            }

            // Credits History Chart (Phase C) — Codex only
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: active ? item.implicitHeight : 0
                active: root.providerId === "codex"
                sourceComponent: CreditsHistoryChart {
                    points: UsageStore.creditsHistoryData()
                }
            }

            // Usage Breakdown Chart (Phase C) — Codex only
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: active ? item.implicitHeight : 0
                active: root.providerId === "codex"
                sourceComponent: UsageBreakdownChart {
                    points: UsageStore.usageBreakdownData(root.providerId)
                }
            }

            // Codex Dashboard Details
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.providerId === "codex"
                spacing: 6

                property var dash: root.providerId === "codex" ? root.dashboard : ({})
                property bool hasDash: dash && dash.creditEvents !== undefined

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#2a2a4a"
                    visible: parent.hasDash
                }

                // Credit Events
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: parent.hasDash && parent.dash.creditEvents && parent.dash.creditEvents.length > 0
                    spacing: 4
                    Text {
                        text: qsTr("Credit Events")
                        color: "#aaa"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Repeater {
                        model: parent.parent.dash.creditEvents || []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: modelData.date ? new Date(modelData.date).toLocaleDateString(Qt.locale(), "yyyy-MM-dd") : ""
                                color: "#888"
                                font.pixelSize: 9
                                Layout.preferredWidth: 70
                            }
                            Text {
                                text: modelData.service || ""
                                color: "#888"
                                font.pixelSize: 9
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: "$" + (modelData.amount || 0).toFixed(2)
                                color: modelData.amount >= 0 ? "#4CAF50" : "#F44336"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                // Usage by Service
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: parent.hasDash && parent.dash.usageByService && parent.dash.usageByService.length > 0
                    spacing: 4
                    Text {
                        text: qsTr("Usage by Service")
                        color: "#aaa"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Repeater {
                        model: parent.parent.dash.usageByService || []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: modelData.service || ""
                                color: "#888"
                                font.pixelSize: 9
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: fmtNum(modelData.tokens || 0) + " tk"
                                color: "#aaa"
                                font.pixelSize: 9
                            }
                            Text {
                                text: "$" + (modelData.costUSD || 0).toFixed(2)
                                color: "#aaa"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                // Purchase URL
                Text {
                    Layout.fillWidth: true
                    visible: parent.hasDash && parent.dash.purchaseURL
                    text: "<a href=\"" + (parent.dash.purchaseURL || "") + "\">" + qsTr("Purchase credits") + "</a>"
                    color: "#64B5F6"
                    font.pixelSize: 10
                    textFormat: Text.RichText
                    onLinkActivated: Qt.openUrlExternally(link)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    text: qsTr("Updated")
                    color: "#888"
                    font.pixelSize: 10
                    Layout.preferredWidth: 80
                }
                Text {
                    text: snap.updatedAt && snap.updatedAt > 0
                        ? new Date(snap.updatedAt).toLocaleTimeString(Qt.locale(), "hh:mm:ss")
                        : "-"
                    color: "#666"
                    font.pixelSize: 10
                    Layout.fillWidth: true
                }
            }

            // Status Page action
            RowLayout {
                Layout.fillWidth: true
                visible: root.statusUrl !== ""
                spacing: 6
                ActionButton {
                    text: qsTr("Status")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    onClicked: {
                        if (root.statusUrl) AppController.openExternalUrl(root.statusUrl)
                    }
                }
            }
        }
    }

    function fmtNum(n) {
        if (n === undefined || n === null) return "0"
        if (n >= 1000000) return (n / 1000000).toFixed(1) + "M"
        if (n >= 1000) return (n / 1000).toFixed(1) + "K"
        return n.toString()
    }

    function timeAgo(ms) {
        if (!ms || ms <= 0) return qsTr("never")
        var ago = Date.now() - ms
        if (ago < 60000) return qsTr("just now")
        if (ago < 3600000) return Math.floor(ago / 60000) + qsTr("m ago")
        if (ago < 86400000) return Math.floor(ago / 3600000) + qsTr("h ago")
        return Math.floor(ago / 86400000) + qsTr("d ago")
    }

    property var brandColors: {
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
    }

    function brandColorFor(providerId) {
        return brandColors[providerId] || "#4A90D9"
    }

    component ActionButton: Rectangle {
        property string text: ""
        property color textColor: "#aaa"
        property color hoverColor: "#3a3a5c"
        signal clicked()

        radius: 6
        color: btnMouse.containsMouse ? hoverColor : "transparent"
        Behavior on color { ColorAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: parent.text
            color: parent.textColor
            font.pixelSize: 12
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}
