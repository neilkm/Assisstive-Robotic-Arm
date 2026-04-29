import QtQuick 2.15
import QtQuick.Window 2.15
import QtMultimedia
import "qrc:/JetsonQtApp/qml" as Shared

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    visibility: Window.FullScreen
    title: "Jetson Nano State UI"
    color: theme ? theme.mainBgColor : "#101820"

    Loader {
        id: themeLoader
        source: "qrc:/JetsonQtApp/theme.qml"
    }

    readonly property QtObject theme: themeLoader.item

    function triggerSelectedAction() {
        stateMachine.triggerSelectedAction()
    }

    Item {
        id: keyTarget
        anchors.fill: parent
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Up) {
                stateMachine.selectPreviousAction()
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                stateMachine.selectNextAction()
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                stateMachine.triggerSelectedAction()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                Qt.quit()
                event.accepted = true
            }
        }

        Rectangle {
            id: headerBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 96
            color: theme.menuBgColor
            border.color: theme.mainBorderColor
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: stateMachine.currentStateName
                color: theme.mainFgColor
                font.family: theme.mainFont
                font.pixelSize: theme.heading_FontSize
                font.bold: true
            }
        }

        Text {
            id: instructionsLabel
            anchors.left: parent.left
            // anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 32
            height: 20
            text: "Up/Down: switch selected action    Return: select    Esc: quit app"
            color: theme.mainBorderColor
            font.family: theme.alternateFont
            font.pixelSize: theme.bodyText_FontSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.NoWrap
        }

        Column {
            id: leftPanel
            anchors.left: parent.left
            anchors.top: headerBar.bottom
            anchors.bottom: instructionsLabel.top
            anchors.leftMargin: 50
            anchors.topMargin: 46
            anchors.bottomMargin: 32
            width: Math.min(540, rightMediaPanel.x - 100)
            spacing: 18

            Text {
                text: "Pick an action"
                color: theme.menuFGColor
                font.family: theme.mainFont
                font.pixelSize: theme.description_FontSize
                font.bold: true
            }

            ListView {
                id: actionList
                width: parent.width
                height: parent.height - y
                interactive: false
                spacing: 12
                model: stateMachine.actions
                currentIndex: stateMachine.selectedActionIndex

                delegate: Shared.ActionButton {
                    width: actionList.width
                    height: 42
                    text: modelData
                    selected: index === stateMachine.selectedActionIndex
                    theme: root.theme
                    onClicked: {
                        while (stateMachine.selectedActionIndex !== index) {
                            stateMachine.selectNextAction()
                        }
                        stateMachine.triggerSelectedAction()
                        keyTarget.forceActiveFocus()
                    }
                }
            }
        }

        Shared.LevelGauge {
            id: levelGauge
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: parent.height / 4
            width: 120
            height: 360
            visible: stateMachine.currentStateName === "Shaking spice into pot"
                     || stateMachine.currentStateName === "Using utensil"

            readonly property bool spiceMode: stateMachine.currentStateName === "Shaking spice into pot"
            value: spiceMode ? stateMachine.spiceLevel : stateMachine.stirSpeed
            label: spiceMode ? "Level" : "Speed"
            theme: root.theme
        }

        Rectangle {
            id: rightMediaPanel
            anchors.right: parent.right
            anchors.top: headerBar.bottom
            anchors.bottom: instructionsLabel.top
            anchors.rightMargin: 50
            anchors.topMargin: 46
            anchors.bottomMargin: 32
            width: Math.min(580, parent.width * 0.45)
            color: theme.menuBgColor
            border.color: theme.mainBorderColor
            border.width: 2

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                Rectangle {
                    width: parent.width
                    height: (parent.height - parent.spacing) / 2
                    color: "transparent"
                    border.width: 0
                    radius: 8
                    clip: true

                    Image {
                        id: stateImage
                        anchors.fill: parent
                        source: stateMachine.imageSource
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        visible: source.toString().length > 0
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !stateImage.visible
                        text: "No image yet for this state"
                        color: theme.mainBorderColor
                        font.family: theme.mainFont
                        font.pixelSize: theme.description_FontSize
                        font.bold: true
                    }
                }

                Rectangle {
                    id: cameraPanel
                    width: parent.width
                    height: (parent.height - parent.spacing) / 2
                    color: "#05080c"
                    border.color: theme.highlightedBorderColor
                    border.width: 1
                    radius: 8
                    clip: true

                    MediaDevices {
                        id: mediaDevices
                    }

                    CaptureSession {
                        id: captureSession
                        camera: Camera {
                            id: systemCamera
                            cameraDevice: mediaDevices.defaultVideoInput
                            active: true
                        }
                        videoOutput: cameraOutput
                    }

                    VideoOutput {
                        id: cameraOutput
                        anchors.fill: parent
                        fillMode: VideoOutput.PreserveAspectCrop
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.right: parent.right
                        height: 42
                        color: "#9905080c"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.leftMargin: 16
                        anchors.topMargin: 12
                        text: "Live camera"
                        color: theme.highlightedBorderColor
                        font.family: theme.alternateFont
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: mediaDevices.defaultVideoInput.id.length === 0
                        text: "No camera detected"
                        color: theme.mainBorderColor
                        font.family: theme.mainFont
                        font.pixelSize: theme.bodyText_FontSize
                        font.bold: true
                    }
                }
            }
        }
    }

    Component.onCompleted: keyTarget.forceActiveFocus()
}
