import QtQuick 2.15
import QtQuick.Layouts 1.15

import CodexBarX 1.0

Rectangle {
    id: root

    property var points: []
    property color barColor: "#49A3B0"

    readonly property int hoveredIndex: chartHover.hoveredIndex
    readonly property bool hasData: points.length > 0

    color: "#1c1c32"
    radius: 8
    implicitWidth: 276
    implicitHeight: hasData ? 130 : 40
    clip: true

    Text {
        anchors.centerIn: parent
        text: qsTr("No credits history data")
        color: "#555"
        font.pixelSize: 10
        visible: !hasData
    }

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

                var maxVal = 0
                for (var i = 0; i < points.length; i++) {
                    var v = points[i].creditsUsed || 0
                    if (v > maxVal) maxVal = v
                }
                if (maxVal <= 0) maxVal = 1

                for (var i = 0; i < points.length; i++) {
                    var x = plotLeft + i * (barW + barGap)
                    var pct = (points[i].creditsUsed || 0) / maxVal
                    var barH = Math.max(pct > 0 ? 1 : 0, pct * plotH)

                    ctx.fillStyle = "#2a2a4a"
                    ctx.fillRect(x, plotBottom - plotH, barW, plotH)
                    ctx.fillStyle = root.barColor
                    ctx.fillRect(x, plotBottom - barH, barW, barH)

                    if (points[i].isPeak === true) {
                        ctx.fillStyle = "#FFC107"
                        ctx.fillRect(x, plotBottom - barH - 1, barW, 3)
                    }
                    if (chartHover.hoveredIndex === i) {
                        ctx.fillStyle = "rgba(255,255,255,0.12)"
                        ctx.fillRect(x, plotBottom - plotH, barW, plotH)
                    }
                }

                if (points.length > 0) {
                    ctx.fillStyle = "#444"
                    ctx.font = "8px sans-serif"
                    ctx.textAlign = "left"
                    ctx.fillText(formatDate(points[0].date), plotLeft, plotBottom + 12)
                    ctx.textAlign = "right"
                    ctx.fillText(formatDate(points[points.length - 1].date), plotRight, plotBottom + 12)
                }
            }

            function formatDate(ds) {
                if (!ds) return ""
                var p = ds.split("-"), m = parseInt(p[1]) - 1, d = parseInt(p[2])
                var ms = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]
                return (m >= 0 && m < 12) ? ms[m] + " " + d : ds
            }

            MouseArea {
                id: chartHover
                anchors.fill: parent
                hoverEnabled: true
                property int hoveredIndex: -1
                property real hoverX: 0
                property real hoverY: 0

                onPositionChanged: {
                    hoverX = mouseX; hoverY = mouseY
                    if (points.length === 0) { hoveredIndex = -1; canvas.requestPaint(); return }
                    var barW = Math.max(2, (canvas.width - 20 - 2 * (points.length - 1)) / points.length)
                    var idx = Math.floor((mouseX - 10) / (barW + 2))
                    hoveredIndex = (idx >= 0 && idx < points.length) ? idx : -1
                    canvas.requestPaint()
                }
                onExited: { hoveredIndex = -1; canvas.requestPaint() }
                cursorShape: Qt.PointingHandCursor
            }

            ChartHoverDetail {
                id: hoverDetail
                floating: true
                accentColor: root.barColor
                visible: hoveredIndex >= 0 && activePoint !== null
                width: implicitWidth
                height: implicitHeight
                property var activePoint: hoveredIndex >= 0 && hoveredIndex < points.length ? points[hoveredIndex] : null

                x: Math.max(4, Math.min(chartHover.hoverX + 10, canvas.width - width - 4))
                y: {
                    var above = chartHover.hoverY - height - 10
                    if (above >= 4) return above
                    return Math.max(4, Math.min(chartHover.hoverY + 12, canvas.height - height - 4))
                }
                primaryText: activePoint
                    ? canvas.formatDate(activePoint.date || "") + ": " + (activePoint.creditsUsed || 0).toFixed(2) + " credits"
                    : ""
                secondaryText: activePoint && activePoint.services && activePoint.services.length > 0
                    ? activePoint.services.join(" · ") : ""
            }
        }
    }
}
