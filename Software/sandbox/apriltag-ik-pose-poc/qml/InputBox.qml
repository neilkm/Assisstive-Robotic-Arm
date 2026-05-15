import QtQuick 2.15

Rectangle {
    id: inputRoot
    property QtObject theme: Theme {}
    property alias text: input.text
    property alias inputActiveFocus: input.activeFocus
    property string label: ""
    signal edited(string value)

    height: theme ? theme.inputHeight : 48
    radius: theme ? theme.buttonRadius : 6
    color: theme ? theme.inputColor : "#111316"
    border.color: theme ? theme.borderColor : "#3d444f"
    border.width: 1

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 5
        text: inputRoot.label
        color: theme ? theme.mutedTextColor : "#b9b6aa"
        font.family: theme ? theme.fontFamily : "Arial"
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
        color: theme ? theme.textColor : "#f4f1e8"
        selectedTextColor: "#101820"
        selectionColor: theme ? theme.warningColor : "#f2c94c"
        font.family: theme ? theme.fontFamily : "Arial"
        font.pixelSize: 16
        clip: true
        selectByMouse: true
        onTextEdited: inputRoot.edited(text)
    }
}
