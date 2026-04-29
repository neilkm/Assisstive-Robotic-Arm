import QtQuick 2.15
import QtQuick.Window 2.15
import QtMultimedia

Window {
    id: root
    width: 1280
    height: 720
    visible: true
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
            if (event.key === Qt.Key_Up || event.key === Qt.Key_Left || event.key === Qt.Key_J) {
                stateMachine.selectPreviousAction()
                event.accepted = true
            } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right || event.key === Qt.Key_K) {
                stateMachine.selectNextAction()
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                stateMachine.triggerSelectedAction()
                event.accepted = true
            } else if (event.key === Qt.Key_I) {
                stateMachine.reset()
                event.accepted = true
            } else if (event.key === Qt.Key_Q || event.key === Qt.Key_Escape) {
                Qt.quit()
                event.accepted = true
            }
        }

        Row {
            anchors.fill: parent
            anchors.margins: 50
            spacing: 50

            Column {
                width: 540
                height: parent.height
                spacing: 18

                Text {
                    text: "Current state"
                    color: theme.menuFGColor
                    font.family: theme.mainFont
                    font.pixelSize: theme.description_FontSize
                    font.bold: true
                }

                Text {
                    text: stateMachine.currentStateName
                    color: theme.mainFgColor
                    font.family: theme.mainFont
                    font.pixelSize: theme.heading_FontSize
                    font.bold: true
                    height: 72
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    visible: stateMachine.currentStateName === "Shaking spice into pot"
                    text: "Spice level: " + stateMachine.spiceLevel
                    color: theme.highlightedBorderColor
                    font.family: theme.mainFont
                    font.pixelSize: theme.button1_fontSize
                    font.bold: true
                    height: visible ? 42 : 0
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    visible: stateMachine.currentStateName === "Using utensil"
                    text: "Stir speed: " + stateMachine.stirSpeed
                    color: theme.highlightedBorderColor
                    font.family: theme.mainFont
                    font.pixelSize: theme.button1_fontSize
                    font.bold: true
                    height: visible ? 42 : 0
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: "Allowed actions"
                    color: theme.menuFGColor
                    font.family: theme.mainFont
                    font.pixelSize: theme.description_FontSize
                    font.bold: true
                }

                ListView {
                    id: actionList
                    width: parent.width
                    height: 338
                    interactive: false
                    spacing: 12
                    model: stateMachine.actions
                    currentIndex: stateMachine.selectedActionIndex

                    delegate: Rectangle {
                        width: actionList.width
                        height: 42
                        radius: selected ? theme.button1_radius : theme.button2_radius
                        color: selected ? theme.highlightedColor : theme.button2_mainColor
                        border.color: selected ? theme.highlightedBorderColor : theme.button2_borderColor
                        border.width: 2

                        readonly property bool selected: index === stateMachine.selectedActionIndex

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: selected ? theme.button1_padding : theme.button2_padding
                            anchors.rightMargin: anchors.leftMargin
                            text: modelData
                            color: selected ? theme.button1_textColor : theme.button2_textColor
                            font.family: theme.mainFont
                            font.pixelSize: selected ? theme.button1_fontSize : theme.button2_fontSize
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                        }

                        MouseArea {
                            anchors.fill: parent
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

                Text {
                    width: parent.width
                    text: "Arrow keys: change selected action    Enter: trigger action    i: reset    q or ESC: quit"
                    color: theme.mainBorderColor
                    font.family: theme.alternateFont
                    font.pixelSize: theme.bodyText_FontSize
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                width: 580
                height: 590
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
                        color: "#0b1117"
                        border.color: theme.mainBorderColor
                        border.width: 1
                        radius: 8

                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.leftMargin: 16
                            anchors.topMargin: 12
                            text: "Illustration"
                            color: theme.menuFGColor
                            font.family: theme.alternateFont
                            font.pixelSize: 15
                            font.bold: true
                            z: 1
                        }

                        Image {
                            id: stateImage
                            anchors.centerIn: parent
                            width: parent.width - 32
                            height: parent.height - 52
                            anchors.verticalCenterOffset: 16
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
    }

    Component.onCompleted: keyTarget.forceActiveFocus()
}
