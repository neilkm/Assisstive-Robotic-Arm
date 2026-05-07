import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    visibility: Window.FullScreen
    title: "AprilTag Visualization Test"
    color: "#0b1016"

    readonly property color panelColor: "#131b24"
    readonly property color panelBorder: "#334152"
    readonly property color primaryText: "#edf3f8"
    readonly property color mutedText: "#9aa8b6"
    readonly property color accent: "#7cc7ff"

    function meters(value) {
        return Number(value).toFixed(3)
    }

    function degrees(value) {
        return Number(value).toFixed(1)
    }

    Item {
        id: keyTarget
        anchors.fill: parent
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape) {
                tagVisualization.stop()
                Qt.quit()
                event.accepted = true
            }
        }

        Rectangle {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 64
            color: "#101820"
            border.color: panelBorder
            border.width: 1

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                text: "AprilTag Camera Pose Visualization"
                color: primaryText
                font.pixelSize: 24
                font.bold: true
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 24
                text: tagVisualization.statusText
                color: tagVisualization.cameraReady && tagVisualization.homeFrameAvailable ? accent : "#ffb86b"
                font.pixelSize: 16
                font.bold: true
            }
        }

        Row {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.margins: 18
            spacing: 16

            Rectangle {
                id: cameraPanel
                width: Math.floor(content.width * 0.40)
                height: content.height
                color: panelColor
                border.color: panelBorder
                border.width: 1
                radius: 6
                clip: true

                Text {
                    id: cameraTitle
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 16
                    text: "Camera"
                    color: primaryText
                    font.pixelSize: 18
                    font.bold: true
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: cameraTitle.bottom
                    anchors.bottom: parent.bottom
                    anchors.margins: 16
                    color: "#05080c"
                    border.color: "#223041"
                    border.width: 1
                    clip: true

                    Image {
                        id: cameraFrame
                        anchors.fill: parent
                        source: tagVisualization.cameraImageSource
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: source.toString().length > 0
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !cameraFrame.visible
                        text: "Waiting for camera frame"
                        color: mutedText
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }

            Rectangle {
                id: gridPanel
                width: Math.floor(content.width * 0.38)
                height: content.height
                color: panelColor
                border.color: panelBorder
                border.width: 1
                radius: 6
                clip: true

                Text {
                    id: gridTitle
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 16
                    text: "Home Tag " + tagVisualization.homeTagId + " Coordinates"
                    color: primaryText
                    font.pixelSize: 18
                    font.bold: true
                }

                Canvas {
                    id: gridCanvas
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: gridTitle.bottom
                    anchors.bottom: parent.bottom
                    anchors.margins: 16

                    function project(x, y, z) {
                        var scale = Math.min(width, height) * 0.30
                        var originX = width * 0.42
                        var originY = height * 0.62
                        return {
                            x: originX + x * scale + z * scale * 0.42,
                            y: originY + y * scale - z * scale * 0.22
                        }
                    }

                    function rotateAxis(axis, pitchDeg, yawDeg, rollDeg) {
                        var pitch = pitchDeg * Math.PI / 180.0
                        var yaw = yawDeg * Math.PI / 180.0
                        var roll = rollDeg * Math.PI / 180.0
                        var x = axis.x
                        var y = axis.y
                        var z = axis.z

                        var cy = Math.cos(pitch)
                        var sy = Math.sin(pitch)
                        var y1 = y * cy - z * sy
                        var z1 = y * sy + z * cy

                        var cx = Math.cos(yaw)
                        var sx = Math.sin(yaw)
                        var x2 = x * cx + z1 * sx
                        var z2 = -x * sx + z1 * cx

                        var cz = Math.cos(roll)
                        var sz = Math.sin(roll)
                        return {
                            x: x2 * cz - y1 * sz,
                            y: x2 * sz + y1 * cz,
                            z: z2
                        }
                    }

                    function drawLine(ctx, a, b, color, widthPx) {
                        ctx.beginPath()
                        ctx.moveTo(a.x, a.y)
                        ctx.lineTo(b.x, b.y)
                        ctx.strokeStyle = color
                        ctx.lineWidth = widthPx
                        ctx.stroke()
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = "#091019"
                        ctx.fillRect(0, 0, width, height)

                        for (var i = -4; i <= 4; ++i) {
                            drawLine(ctx, project(i * 0.25, 0, 0), project(i * 0.25, 0, 1.2), "#263545", 1)
                            drawLine(ctx, project(-1.0, 0, i * 0.15 + 0.6), project(1.0, 0, i * 0.15 + 0.6), "#263545", 1)
                        }

                        drawLine(ctx, project(0, 0, 0), project(0.7, 0, 0), "#e85b5b", 3)
                        drawLine(ctx, project(0, 0, 0), project(0, 0.7, 0), "#65c975", 3)
                        drawLine(ctx, project(0, 0, 0), project(0, 0, 0.9), "#5c8dff", 3)

                        ctx.fillStyle = mutedText
                        ctx.font = "13px sans-serif"
                        var lx = project(0.72, 0, 0)
                        var ly = project(0, 0.72, 0)
                        var lz = project(0, 0, 0.92)
                        ctx.fillText("+X", lx.x + 4, lx.y)
                        ctx.fillText("+Y", ly.x + 4, ly.y)
                        ctx.fillText("+Z", lz.x + 4, lz.y)

                        var tags = tagVisualization.tagPoses
                        for (var t = 0; t < tags.length; ++t) {
                            var tag = tags[t]
                            var base = project(tag.x, tag.y, tag.z)
                            var axisLen = 0.10
                            var xAxis = rotateAxis({x: axisLen, y: 0, z: 0}, tag.pitch, tag.yaw, tag.roll)
                            var yAxis = rotateAxis({x: 0, y: axisLen, z: 0}, tag.pitch, tag.yaw, tag.roll)
                            var zAxis = rotateAxis({x: 0, y: 0, z: axisLen}, tag.pitch, tag.yaw, tag.roll)

                            drawLine(ctx, base, project(tag.x + xAxis.x, tag.y + xAxis.y, tag.z + xAxis.z), "#ff6666", 3)
                            drawLine(ctx, base, project(tag.x + yAxis.x, tag.y + yAxis.y, tag.z + yAxis.z), "#78df87", 3)
                            drawLine(ctx, base, project(tag.x + zAxis.x, tag.y + zAxis.y, tag.z + zAxis.z), "#6d96ff", 3)

                            ctx.beginPath()
                            ctx.arc(base.x, base.y, 6, 0, Math.PI * 2)
                            ctx.fillStyle = "#f8d66d"
                            ctx.fill()
                            ctx.strokeStyle = "#161b22"
                            ctx.lineWidth = 2
                            ctx.stroke()
                            ctx.fillStyle = primaryText
                            ctx.font = "bold 13px sans-serif"
                            ctx.fillText("ID " + tag.id, base.x + 9, base.y - 8)
                        }
                    }

                    Connections {
                        target: tagVisualization
                        function onVisualizationChanged() {
                            gridCanvas.requestPaint()
                        }
                    }

                    Component.onCompleted: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                }
            }

            Rectangle {
                id: listPanel
                width: content.width - cameraPanel.width - gridPanel.width - content.spacing * 2
                height: content.height
                color: panelColor
                border.color: panelBorder
                border.width: 1
                radius: 6
                clip: true

                Text {
                    id: listTitle
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 16
                    text: "Tags Relative to Home " + tagVisualization.homeTagId
                    color: primaryText
                    font.pixelSize: 18
                    font.bold: true
                }

                ListView {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: listTitle.bottom
                    anchors.bottom: parent.bottom
                    anchors.margins: 14
                    spacing: 10
                    clip: true
                    model: tagVisualization.tagPoses

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 126
                        color: "#0c131b"
                        border.color: "#2c3a49"
                        border.width: 1
                        radius: 5

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 5

                            Text {
                                text: "AprilTag ID " + modelData.id
                                color: accent
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Text {
                                text: "xyz m:  " + meters(modelData.x) + "  " + meters(modelData.y) + "  " + meters(modelData.z)
                                color: primaryText
                                font.pixelSize: 13
                            }

                            Text {
                                text: "euler deg:  " + degrees(modelData.pitch) + "  " + degrees(modelData.yaw) + "  " + degrees(modelData.roll)
                                color: primaryText
                                font.pixelSize: 13
                            }

                            Text {
                                text: "distance m:  " + meters(modelData.distance)
                                color: mutedText
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: tagVisualization.tagPoses.length === 0
                    text: tagVisualization.homeFrameAvailable ? "No visible AprilTags" : "Home tag " + tagVisualization.homeTagId + " not visible"
                    color: mutedText
                    font.pixelSize: 16
                    font.bold: true
                }
            }
        }

        Rectangle {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 40
            color: "#0b1016"

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                text: "OpenCV camera frame | coordinates relative to AprilTag ID " + tagVisualization.homeTagId + " | Esc quits"
                color: mutedText
                font.pixelSize: 13
            }
        }
    }

    Component.onCompleted: keyTarget.forceActiveFocus()
}
