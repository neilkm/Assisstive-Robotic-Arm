import QtQuick 2.15

Column {
    id: root

    property int value: 1
    property string label: "Level"
    property QtObject theme

    width: 120
    spacing: 8

    Item {
        width: parent.width
        height: parent.height - gaugeLabel.height - parent.spacing

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Repeater {
                model: 5

                Rectangle {
                    width: 34
                    height: 58
                    radius: 6
                    border.width: 2
                    border.color: Qt.rgba(0.9, 0.97, 1.0, 0.72)
                    color: filled ? segmentColor : "transparent"

                    readonly property int segmentLevel: 5 - index
                    readonly property bool filled: segmentLevel <= root.value
                    readonly property color segmentColor: {
                        if (segmentLevel === 1) {
                            return "#ef4444"
                        }
                        if (segmentLevel === 2) {
                            return "#f97316"
                        }
                        if (segmentLevel === 3) {
                            return "#facc15"
                        }
                        if (segmentLevel === 4) {
                            return "#84cc16"
                        }
                        return "#22c55e"
                    }
                }
            }
        }
    }

    Text {
        id: gaugeLabel
        width: parent.width
        text: root.label + ": " + root.value
        color: root.theme ? root.theme.highlightedBorderColor : "#ffe08a"
        font.family: root.theme ? root.theme.mainFont : "Avenir Next"
        font.pixelSize: root.theme ? root.theme.bodyText_FontSize : 22
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
    }
}
