import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property var points: []

    readonly property int hoveredIndex: chartHover.hoveredIndex
    readonly property bool hasData: points.length > 0

    color: "#1c1c32"
    radius: 8
    implicitWidth: 276
    implicitHeight: hasData ? 130 + legendArea.implicitHeight + 8 + (legendArea.visible ? 12 : 0) : 40
    clip: true

    // Service color map
    property var serviceColors: ({
        "CLI": "#4260F0", "GitHub Review": "#F0882E", "API": "#4CAF50",
        "Codex": "#9C27B0", "Codex CLI": "#E91E63", "Dashboard": "#00BCD4",
        "Storage": "#795548"
    })

    function hashColor(name) {
        var h = 0
        for (var i = 0; i < name.length; i++) h = ((h << 5) - h) + name.charCodeAt(i)
        var colors = ["#FF9800", "#607D8B", "#CDDC39", "#3F51B5"]
        return colors[Math.abs(h) % colors.length]
    }

    function svcColor(name) { return serviceColors[name] || hashColor(name) }

    // Build flat legend from all days
    function allLegendItems() {
        var seen = {}
        var items = []
        for (var i = 0; i < points.length; i++) {
            var svcs = points[i].services || []
            for (var j = 0; j < svcs.length; j++) {
                var n = svcs[j].name
                if (!seen[n]) { seen[n] = true; items.push({name: n, color: svcColor(n)}) }
            }
        }
        return items
    }

    Text {
        anchors.centerIn: parent
        text: qsTr("No breakdown data")
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

                // Find max total across all days
                var dailyMax = 0
                for (var i = 0; i < points.length; i++) {
                    var t = points[i].totalCredits || 0
                    if (t > dailyMax) dailyMax = t
                }
                if (dailyMax <= 0) dailyMax = 1

                for (var i = 0; i < points.length; i++) {
                    var x = plotLeft + i * (barW + barGap)
                    var total = points[i].totalCredits || 0
                    var barH = Math.max(total > 0 ? 1 : 0, (total / dailyMax) * plotH)

                    // Track background
                    ctx.fillStyle = "#2a2a4a"
                    ctx.fillRect(x, plotBottom - plotH, barW, plotH)

                    // Stack segments from bottom up
                    var svcs = points[i].services || []
                    var stackY = plotBottom
                    for (var j = 0; j < svcs.length; j++) {
                        var segH = total > 0 ? Math.max(1, (svcs[j].credits / dailyMax) * plotH) : 0
                        ctx.fillStyle = root.svcColor(svcs[j].name)
                        ctx.fillRect(x, stackY - segH, barW, segH)
                        stackY -= segH
                    }

                    // Hover overlay
                    if (chartHover.hoveredIndex === i) {
                        ctx.fillStyle = "rgba(255,255,255,0.12)"
                        ctx.fillRect(x, plotBottom - plotH, barW, plotH)
                    }
                }

                // X-axis
                if (points.length > 0) {
                    ctx.fillStyle = "#444"
                    ctx.font = "8px sans-serif"
                    ctx.textAlign = "left"
                    ctx.fillText(fmtDate(points[0].date), plotLeft, plotBottom + 12)
                    ctx.textAlign = "right"
                    ctx.fillText(fmtDate(points[points.length - 1].date), plotRight, plotBottom + 12)
                }
            }

            function fmtDate(ds) {
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
                accentColor: "#6b6bff"
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
                    ? canvas.fmtDate(activePoint.date || "") + ": " + (activePoint.totalCredits || 0).toFixed(2) + " credits"
                    : ""
                secondaryText: {
                    if (!activePoint) return ""
                    var svcs = activePoint.services || []
                    var lines = []
                    for (var i = 0; i < Math.min(3, svcs.length); i++)
                        lines.push(svcs[i].name + " " + (svcs[i].credits || 0).toFixed(1))
                    return lines.join(" · ")
                }
            }
        }

        // Legend
        ColumnLayout {
            id: legendArea
            Layout.fillWidth: true
            spacing: 4
            visible: legendItems.count > 0

            Text {
                text: qsTr("Services")
                color: "#888"
                font.pixelSize: 10
                font.bold: true
            }

            Flow {
                Layout.fillWidth: true
                spacing: 6
                property var legendItems: root.allLegendItems()

                Repeater {
                    id: legendItems
                    model: parent.legendItems

                    RowLayout {
                        spacing: 4
                        Rectangle {
                            width: 10; height: 10; radius: 3
                            color: modelData.color
                        }
                        Text {
                            text: modelData.name
                            color: "#888"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }
}
