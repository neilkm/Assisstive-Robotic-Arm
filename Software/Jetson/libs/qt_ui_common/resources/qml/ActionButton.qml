import QtQuick 2.15

Rectangle {
    id: root

    property string text: ""
    property bool selected: false
    property QtObject theme

    signal clicked()

    width: 360
    height: 42
    radius: selected ? (theme ? theme.button1_radius : 10) : (theme ? theme.button2_radius : 10)
    color: selected ? (theme ? theme.highlightedColor : "#f2c14e") : (theme ? theme.button2_mainColor : "#263241")
    border.color: selected ? (theme ? theme.highlightedBorderColor : "#ffe08a") : (theme ? theme.button2_borderColor : "#496174")
    border.width: 2

    Text {
        anchors.fill: parent
        anchors.leftMargin: root.selected ? (root.theme ? root.theme.button1_padding : 18) : (root.theme ? root.theme.button2_padding : 18)
        anchors.rightMargin: anchors.leftMargin
        text: root.text
        color: root.selected ? (root.theme ? root.theme.button1_textColor : "#062925") : (root.theme ? root.theme.button2_textColor : "#e8f3f8")
        font.family: root.theme ? root.theme.mainFont : "Avenir Next"
        font.pixelSize: root.selected ? (root.theme ? root.theme.button1_fontSize : 26) : (root.theme ? root.theme.button2_fontSize : 26)
        font.bold: true
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
