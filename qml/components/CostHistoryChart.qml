import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

Rectangle {
    id: root

    property string providerId: ""
    property var points: []
    property color barColor: brandColorFor(providerId)
    property bool ready: false

    readonly property int hoveredIndex: chartHover.hoveredIndex
    readonly property bool hasData: points.length > 0

    color: "#1c1c32"
    radius: 8
    implicitWidth: 276
    implicitHeight: hasData ? 130 + hoverDetail.implicitHeight + 16 : 40
    clip: true

    function brandColorFor(pid) {
        var colors = {
            "codex": "#49A3B0", "claude": "#CC7C5E", "cursor": "#5B8DFA",
            "gemini": "#8860D0", "copilot": "#2DA44E", "zai": "#E85A6A",
            "opencode": "#E44D26", "warp": "#00BCD4", "mistral": "#F77F00",
            "openrouter": "#FF6B6B", "ollama": "#E6EF6C", "kilo": "#7C3AED",
            "deepseek": "#4D6BFE", "codebuff": "#44FF00", "perplexity": "#22C55E",
            "kimi": "#8B5CF6", "abacus": "#6366F1", "alibaba": "#F97316",
            "augment": "#14B8A6", "amp": "#D946EF", "factory": "#84CC16",
            "jetbrains": "#F000F0", "vertexai": "#4285F4", "windsurf": "#34E8BB",
            "minimax": "#EC4899", "synthetic": "#6366F1", "antigravity": "#10B981",
            "opencodego": "#3B82F6"
        }
        return colors[pid] || "#4A90D9"
    }

    function formatDetailDate(dateStr) {
        if (!dateStr) return ""
        var parts = dateStr.split("-")
        if (parts.length < 3) return dateStr
        var months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
        var m = parseInt(parts[1]) - 1
        var d = parseInt(parts[2])
        return (m >= 0 && m < 12) ? months[m] + " " + d : dateStr
    }

    function modelCostSummary(point) {
        if (!point) return ""
        var models = point.models || []
        var lines = []
        for (var i = 0; i < Math.min(4, models.length); i++)
            lines.push(models[i].name + " $" + (models[i].costUSD || 0).toFixed(2))
        return lines.join("  ")
    }

    function refreshPoints() {
        if (!root.ready) {
            return
        }
        if (!root.providerId) {
            root.points = []
            return
        }

        root.points = UsageStore.costHistoryChartData(root.providerId)
        canvas.requestPaint()
    }

    // No data state
    Text {
        anchors.centerIn: parent
        text: qsTr("No cost history data")
        color: "#555"
        font.pixelSize: 10
        visible: !hasData
    }

    // Chart area
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4
        visible: hasData

        Canvas {
            id: canvas
            Layout.fillWidth: true
            Layout.preferredHeight: 110

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)

                if (points.length === 0) return

                var plotLeft = 10, plotRight = width - 10
                var plotTop = 6, plotBottom = height - 14
                var plotW = plotRight - plotLeft
                var plotH = plotBottom - plotTop

                var barGap = 2
                var barW = Math.max(2, (plotW - barGap * (points.length - 1)) / points.length)

                // Find max value for scaling
                var maxVal = 0
                for (var i = 0; i < points.length; i++) {
                    var v = points[i].costUSD || 0
                    if (v > maxVal) maxVal = v
                }
                if (maxVal <= 0) maxVal = 1

                // Draw bars
                for (var i = 0; i < points.length; i++) {
                    var x = plotLeft + i * (barW + barGap)
                    var pct = (points[i].costUSD || 0) / maxVal
                    var barH = Math.max(pct > 0 ? 1 : 0, pct * plotH)

                    // Bar background track
                    ctx.fillStyle = "#2a2a4a"
                    ctx.fillRect(x, plotBottom - plotH, barW, plotH)

                    // Bar fill
                    ctx.fillStyle = root.barColor
                    ctx.fillRect(x, plotBottom - barH, barW, barH)

                    // Peak marker (yellow cap for highest bar)
                    if (points[i].isPeak === true) {
                        ctx.fillStyle = "#FFC107"
                        ctx.fillRect(x, plotBottom - barH - 1, barW, 3)
                    }

                    // Hover overlay
                    if (chartHover.hoveredIndex === i) {
                        ctx.fillStyle = "rgba(255,255,255,0.12)"
                        ctx.fillRect(x, plotBottom - plotH, barW, plotH)
                    }
                }

                // X-axis labels (first and last)
                if (points.length > 0) {
                    ctx.fillStyle = "#444"
                    ctx.font = "8px sans-serif"
                    ctx.textAlign = "left"
                    var firstLabel = formatShortDate(points[0].date)
                    ctx.fillText(firstLabel, plotLeft, plotBottom + 12)
                    ctx.textAlign = "right"
                    var lastLabel = formatShortDate(points[points.length - 1].date)
                    ctx.fillText(lastLabel, plotRight, plotBottom + 12)
                }
            }

            function formatShortDate(dateStr) {
                if (!dateStr) return ""
                var parts = dateStr.split("-")
                if (parts.length < 2) return dateStr
                var months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
                var m = parseInt(parts[1]) - 1
                var d = parseInt(parts[2])
                if (m >= 0 && m < 12) return months[m] + " " + d
                return dateStr
            }

            // Hover tracking
            MouseArea {
                id: chartHover
                anchors.fill: parent
                hoverEnabled: true
                property int hoveredIndex: -1

                onPositionChanged: {
                    if (points.length === 0) return
                    var plotW = canvas.width - 20
                    var barGap = 2
                    var barW = Math.max(2, (plotW - barGap * (points.length - 1)) / points.length)
                    var idx = Math.floor((mouseX - 10) / (barW + barGap))
                    if (idx >= 0 && idx < points.length) {
                        hoveredIndex = idx
                    } else {
                        hoveredIndex = -1
                    }
                    canvas.requestPaint()
                }
                onExited: {
                    hoveredIndex = -1
                    canvas.requestPaint()
                }
                cursorShape: Qt.PointingHandCursor
            }
        }

        ChartHoverDetail {
            id: hoverDetail
            property int activeIdx: hoveredIndex >= 0 ? hoveredIndex : (points.length > 0 ? points.length - 1 : -1)
            property var activePoint: activeIdx >= 0 && activeIdx < points.length ? points[activeIdx] : null

            primaryText: activePoint ? root.formatDetailDate(activePoint.date || "") + ": $" + (activePoint.costUSD || 0).toFixed(2) : ""
            secondaryText: root.modelCostSummary(activePoint)
        }
    }

    Component.onCompleted: {
        root.ready = true
        root.refreshPoints()
    }
    onProviderIdChanged: root.refreshPoints()

    Connections {
        target: UsageStore
        function onCostUsageChanged() {
            root.refreshPoints()
        }
        function onCostHistoryChanged() {
            root.refreshPoints()
        }
    }
}
