import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: root
    objectName: "rootWindow"
    width: 1280
    height: 760
    minimumWidth: 1040
    minimumHeight: 680
    visible: true
    title: "AprilTag IK Pose POC"
    color: "#151719"

    property string currentPage: "pose"
    property color panelColor: "#202327"
    property color borderColor: "#3d444f"
    property color textColor: "#f4f1e8"
    property color mutedTextColor: "#b9b6aa"
    property color actionColor: "#2f80ed"
    property color warningColor: "#f2c94c"
    property color purpleColor: "#9b5de5"
    property color greenColor: "#27ae60"
    property color redColor: "#eb5757"

    function fixed(value, digits) {
        return Number(value).toFixed(digits)
    }

    function pointText(point) {
        return fixed(point.x, 3) + ", " + fixed(point.y, 3) + ", " + fixed(point.z, 3)
    }

    function loadConfigModels() {
        dimensionModel.clear()
        var dimensions = poseController.dimensions
        for (var i = 0; i < dimensions.length; ++i) {
            dimensionModel.append({
                name: String(dimensions[i].name),
                value: Number(dimensions[i].value).toFixed(6)
            })
        }

        dhModel.clear()
        var rows = poseController.dhRows
        for (var j = 0; j < rows.length; ++j) {
            dhModel.append({
                name: String(rows[j].name),
                a_m: String(rows[j].a_m),
                alpha_rad: String(rows[j].alpha_rad),
                d_m: String(rows[j].d_m),
                theta_offset_rad: String(rows[j].theta_offset_rad),
                initial_deg: String(rows[j].initial_deg),
                min_deg: String(rows[j].min_deg),
                max_deg: String(rows[j].max_deg)
            })
        }
    }

    function saveConfigModels() {
        var dimensions = []
        for (var i = 0; i < dimensionModel.count; ++i) {
            var dim = dimensionModel.get(i)
            dimensions.push({name: dim.name, value: dim.value})
        }

        var dhRows = []
        for (var j = 0; j < dhModel.count; ++j) {
            var row = dhModel.get(j)
            dhRows.push({
                name: row.name,
                a_m: row.a_m,
                alpha_rad: row.alpha_rad,
                d_m: row.d_m,
                theta_offset_rad: row.theta_offset_rad,
                initial_deg: row.initial_deg,
                min_deg: row.min_deg,
                max_deg: row.max_deg
            })
        }
        poseController.saveConfig(dimensions, dhRows)
    }

    ListModel {
        id: dimensionModel
    }

    ListModel {
        id: dhModel
    }

    component ToolButton: Rectangle {
        id: buttonRoot
        property alias text: label.text
        property color fillColor: root.actionColor
        property color textColor: "#ffffff"
        signal clicked()

        radius: 6
        height: 42
        color: enabled ? fillColor : "#34383e"
        border.color: enabled ? Qt.lighter(fillColor, 1.25) : root.borderColor
        border.width: 1
        opacity: enabled ? 1.0 : 0.62

        Text {
            id: label
            anchors.centerIn: parent
            color: buttonRoot.textColor
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            width: parent.width - 16
        }

        MouseArea {
            anchors.fill: parent
            enabled: buttonRoot.enabled
            onClicked: buttonRoot.clicked()
        }
    }

    component InputBox: Rectangle {
        id: inputRoot
        property alias text: input.text
        property string label: ""
        signal edited(string value)

        height: 48
        radius: 6
        color: "#111316"
        border.color: root.borderColor
        border.width: 1

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.top: parent.top
            anchors.topMargin: 5
            text: inputRoot.label
            color: root.mutedTextColor
            font.pixelSize: 10
            font.bold: true
        }

        TextInput {
            id: input
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.bottomMargin: 5
            height: 24
            color: root.textColor
            selectedTextColor: "#101820"
            selectionColor: root.warningColor
            font.pixelSize: 16
            clip: true
            selectByMouse: true
            onTextChanged: inputRoot.edited(text)
        }
    }

    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 64
        color: "#111316"
        border.color: root.borderColor
        border.width: 1

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            ToolButton {
                objectName: "posePageButton"
                width: 96
                text: "Pose"
                fillColor: root.currentPage === "pose" ? root.actionColor : "#2b3036"
                onClicked: root.currentPage = "pose"
            }

            ToolButton {
                objectName: "configPageButton"
                width: 96
                text: "Config"
                fillColor: root.currentPage === "config" ? root.actionColor : "#2b3036"
                onClicked: root.currentPage = "config"
            }
        }

        Text {
            anchors.centerIn: parent
            text: "AprilTag IK Pose Proof Of Concept"
            color: root.textColor
            font.pixelSize: 22
            font.bold: true
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: poseController.moving ? "Moving" : "Idle"
            color: poseController.moving ? root.warningColor : root.greenColor
            font.pixelSize: 14
            font.bold: true
        }
    }

    Item {
        id: posePage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        visible: root.currentPage === "pose"

        Canvas {
            id: armCanvas
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width - sidePanel.width
            antialiasing: true

            property real azimuthDeg: -45
            property real elevationDeg: 24
            property real lastDragX: 0

            function project(point) {
                var limit = Math.max(Number(poseController.axisLimit), 12.0)
                var scale = Math.min(width, height) * 0.42 / limit
                var az = azimuthDeg * Math.PI / 180.0
                var el = elevationDeg * Math.PI / 180.0
                var cosA = Math.cos(az)
                var sinA = Math.sin(az)
                var cosE = Math.cos(el)
                var sinE = Math.sin(el)
                var x = point.x * cosA - point.y * sinA
                var y = point.x * sinA + point.y * cosA
                var z = point.z
                var y2 = y * cosE - z * sinE
                var z2 = y * sinE + z * cosE
                return {
                    x: width * 0.5 + x * scale,
                    y: height * 0.58 - z2 * scale,
                    depth: y2
                }
            }

            function drawLine(ctx, a, b, color, lineWidth) {
                var pa = project(a)
                var pb = project(b)
                ctx.beginPath()
                ctx.moveTo(pa.x, pa.y)
                ctx.lineTo(pb.x, pb.y)
                ctx.strokeStyle = color
                ctx.lineWidth = lineWidth
                ctx.stroke()
            }

            function drawPoint(ctx, point, radius, color, label) {
                var p = project(point)
                ctx.beginPath()
                ctx.arc(p.x, p.y, radius, 0, Math.PI * 2)
                ctx.fillStyle = color
                ctx.fill()
                ctx.lineWidth = 1.5
                ctx.strokeStyle = "#0b0d10"
                ctx.stroke()
                if (label && label.length > 0) {
                    ctx.fillStyle = root.textColor
                    ctx.font = "11px sans-serif"
                    ctx.fillText(label, p.x + radius + 4, p.y - radius - 2)
                }
            }

            function drawCube(ctx) {
                var chain = poseController.chainPoints
                if (chain.length === 0) {
                    return
                }
                var base = chain[0]
                var h = 6.0
                var s = 12.0
                var v = [
                    {x: base.x - h, y: base.y - h, z: base.z},
                    {x: base.x + h, y: base.y - h, z: base.z},
                    {x: base.x + h, y: base.y + h, z: base.z},
                    {x: base.x - h, y: base.y + h, z: base.z},
                    {x: base.x - h, y: base.y - h, z: base.z + s},
                    {x: base.x + h, y: base.y - h, z: base.z + s},
                    {x: base.x + h, y: base.y + h, z: base.z + s},
                    {x: base.x - h, y: base.y + h, z: base.z + s}
                ]
                var edges = [[0,1], [1,2], [2,3], [3,0], [4,5], [5,6], [6,7], [7,4], [0,4], [1,5], [2,6], [3,7]]
                for (var i = 0; i < edges.length; ++i) {
                    drawLine(ctx, v[edges[i][0]], v[edges[i][1]], "#4d5967", 1.5)
                }
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "#171b1f"
                ctx.fillRect(0, 0, width, height)

                var gridLimit = Math.max(Number(poseController.axisLimit), 12.0)
                var step = Math.max(1.0, Math.round(gridLimit / 6.0))
                for (var i = -gridLimit; i <= gridLimit + 0.001; i += step) {
                    drawLine(ctx, {x: -gridLimit, y: i, z: 0}, {x: gridLimit, y: i, z: 0}, "#2c333b", 1)
                    drawLine(ctx, {x: i, y: -gridLimit, z: 0}, {x: i, y: gridLimit, z: 0}, "#2c333b", 1)
                }
                drawLine(ctx, {x: -gridLimit, y: 0, z: 0}, {x: gridLimit, y: 0, z: 0}, "#506073", 2)
                drawLine(ctx, {x: 0, y: -gridLimit, z: 0}, {x: 0, y: gridLimit, z: 0}, "#506073", 2)
                drawLine(ctx, {x: 0, y: 0, z: 0}, {x: 0, y: 0, z: gridLimit}, "#506073", 2)

                drawCube(ctx)

                var chain = poseController.chainPoints
                for (var c = 0; c + 1 < chain.length; ++c) {
                    drawLine(ctx, chain[c], chain[c + 1], root.warningColor, 4)
                }

                for (var j = 0; j < chain.length - 1; ++j) {
                    drawPoint(ctx, chain[j], 7, root.actionColor, chain[j].label)
                }

                var tags = poseController.tagPoints
                for (var t = 0; t < tags.length; ++t) {
                    drawPoint(ctx, tags[t], 6, root.purpleColor, "")
                }

                drawPoint(ctx, poseController.actualEndEffector, 8, root.redColor, "FK")
                drawPoint(ctx, poseController.tagEndEffector, 8, root.greenColor, "Tag")

                var target = project(poseController.targetPoint)
                ctx.strokeStyle = "#ffffff"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(target.x - 8, target.y - 8)
                ctx.lineTo(target.x + 8, target.y + 8)
                ctx.moveTo(target.x + 8, target.y - 8)
                ctx.lineTo(target.x - 8, target.y + 8)
                ctx.stroke()
            }

            MouseArea {
                anchors.fill: parent
                onPressed: armCanvas.lastDragX = mouse.x
                onPositionChanged: {
                    if (pressed) {
                        armCanvas.azimuthDeg -= (mouse.x - armCanvas.lastDragX) * 0.35
                        armCanvas.lastDragX = mouse.x
                        armCanvas.requestPaint()
                    }
                }
            }

            Connections {
                target: poseController
                function onPoseChanged() { armCanvas.requestPaint() }
                function onConfigChanged() { armCanvas.requestPaint() }
            }

            onAzimuthDegChanged: requestPaint()
            onElevationDegChanged: requestPaint()
        }

        Rectangle {
            id: sidePanel
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 390
            color: root.panelColor
            border.color: root.borderColor
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 14

                Text {
                    width: parent.width
                    text: "Target XYZ"
                    color: root.textColor
                    font.pixelSize: 18
                    font.bold: true
                }

                Row {
                    width: parent.width
                    spacing: 8

                    InputBox {
                        id: xInput
                        width: (parent.width - 16) / 3
                        label: "X"
                        text: "3.000"
                    }

                    InputBox {
                        id: yInput
                        width: (parent.width - 16) / 3
                        label: "Y"
                        text: "0.000"
                    }

                    InputBox {
                        id: zInput
                        width: (parent.width - 16) / 3
                        label: "Z"
                        text: "8.000"
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8

                    ToolButton {
                        objectName: "moveButton"
                        width: (parent.width - 16) / 3
                        text: "Move"
                        enabled: !poseController.moving
                        onClicked: poseController.moveToTarget(Number(xInput.text), Number(yInput.text), Number(zInput.text))
                    }

                    ToolButton {
                        objectName: "zeroButton"
                        width: (parent.width - 16) / 3
                        text: "Zero"
                        fillColor: "#5f6b7a"
                        onClicked: poseController.resetZero()
                    }

                    ToolButton {
                        objectName: "cubeButton"
                        width: (parent.width - 16) / 3
                        text: "Cube"
                        fillColor: root.purpleColor
                        enabled: !poseController.moving && !poseController.cubeTestRunning
                        onClicked: poseController.runCubeTest()
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: root.borderColor
                }

                Text {
                    width: parent.width
                    text: "Solvers"
                    color: root.textColor
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: "IK: " + poseController.ikConverged + "   error " + fixed(poseController.ikErrorM, 5) + " m\n"
                          + "Tag: " + poseController.tagConverged + "   RMS " + fixed(poseController.tagRmsErrorM, 5) + " m\n"
                          + "EE diff: " + fixed(poseController.endEffectorCompareErrorM, 5) + " m"
                    color: root.mutedTextColor
                    font.pixelSize: 14
                    lineHeight: 1.25
                }

                Text {
                    width: parent.width
                    text: "End Effector"
                    color: root.textColor
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: "FK:  " + pointText(poseController.actualEndEffector) + "\n"
                          + "Tag: " + pointText(poseController.tagEndEffector) + "\n"
                          + "Target: " + pointText(poseController.targetPoint)
                    color: root.mutedTextColor
                    font.pixelSize: 14
                    lineHeight: 1.25
                }

                Text {
                    width: parent.width
                    text: "Joint Angles"
                    color: root.textColor
                    font.pixelSize: 18
                    font.bold: true
                }

                ListView {
                    width: parent.width
                    height: 168
                    interactive: false
                    model: poseController.jointAngles
                    spacing: 4

                    delegate: Row {
                        width: ListView.view.width
                        height: 22
                        spacing: 8

                        Text {
                            width: 160
                            text: modelData.name
                            color: root.mutedTextColor
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Text {
                            width: 86
                            text: fixed(modelData.angleDeg, 2) + " deg"
                            color: root.textColor
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                        }

                        Text {
                            width: 86
                            text: fixed(modelData.tagAngleDeg, 2) + " tag"
                            color: root.greenColor
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Text {
                    width: parent.width
                    text: poseController.cubeTestRunning
                          ? "Cube test step " + poseController.cubeTestStep + " of " + poseController.cubeTestStepCount
                          : poseController.message
                    color: poseController.cubeTestRunning ? root.warningColor : root.mutedTextColor
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Item {
        id: configPage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        visible: root.currentPage === "config"

        Rectangle {
            anchors.fill: parent
            color: "#171b1f"
        }

        Row {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 22

            Rectangle {
                width: 290
                height: parent.height
                color: root.panelColor
                border.color: root.borderColor
                border.width: 1
                radius: 6

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        width: parent.width
                        text: "Dimensions"
                        color: root.textColor
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Repeater {
                        model: dimensionModel
                        delegate: Row {
                            width: parent.width
                            height: 40
                            spacing: 10

                            Text {
                                width: 54
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.name
                                color: root.mutedTextColor
                                font.pixelSize: 13
                                font.bold: true
                            }

                            InputBox {
                                width: parent.width - 64
                                height: 40
                                label: ""
                                text: model.value
                                onEdited: function(value) { dimensionModel.setProperty(index, "value", value) }
                            }
                        }
                    }

                    Item {
                        width: 1
                        height: 8
                    }

                    ToolButton {
                        objectName: "saveConfigButton"
                        width: parent.width
                        text: "Save Config"
                        onClicked: root.saveConfigModels()
                    }

                    ToolButton {
                        objectName: "reloadConfigButton"
                        width: parent.width
                        text: "Reload CSV"
                        fillColor: "#5f6b7a"
                        onClicked: poseController.reloadConfig()
                    }
                }
            }

            Rectangle {
                width: parent.width - 312
                height: parent.height
                color: root.panelColor
                border.color: root.borderColor
                border.width: 1
                radius: 6

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        width: parent.width
                        text: "DH Table"
                        color: root.textColor
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Row {
                        width: parent.width
                        height: 22
                        spacing: 6
                        Repeater {
                            model: ["name", "a", "alpha", "d", "offset", "init", "min", "max"]
                            delegate: Text {
                                width: index === 0 ? 180 : (parent.width - 180 - 42) / 7
                                text: modelData
                                color: root.mutedTextColor
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                    }

                    Repeater {
                        model: dhModel
                        delegate: Row {
                            width: parent.width
                            height: 42
                            spacing: 6

                            InputBox {
                                width: 180
                                height: 40
                                text: model.name
                                onEdited: function(value) { dhModel.setProperty(index, "name", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.a_m
                                onEdited: function(value) { dhModel.setProperty(index, "a_m", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.alpha_rad
                                onEdited: function(value) { dhModel.setProperty(index, "alpha_rad", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.d_m
                                onEdited: function(value) { dhModel.setProperty(index, "d_m", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.theta_offset_rad
                                onEdited: function(value) { dhModel.setProperty(index, "theta_offset_rad", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.initial_deg
                                onEdited: function(value) { dhModel.setProperty(index, "initial_deg", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.min_deg
                                onEdited: function(value) { dhModel.setProperty(index, "min_deg", value) }
                            }

                            InputBox {
                                width: (parent.width - 180 - 42) / 7
                                height: 40
                                text: model.max_deg
                                onEdited: function(value) { dhModel.setProperty(index, "max_deg", value) }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: poseController.message
                        color: root.mutedTextColor
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Connections {
        target: poseController
        function onConfigChanged() { root.loadConfigModels() }
    }

    Component.onCompleted: {
        root.loadConfigModels()
        armCanvas.requestPaint()
    }
}
