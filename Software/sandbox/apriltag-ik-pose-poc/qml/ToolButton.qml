import QtQuick 2.15

Rectangle {
    id: buttonRoot
    property QtObject theme: Theme {}
    property alias text: label.text
    property color fillColor: theme ? theme.actionColor : "#2f80ed"
    property color textColor: "#ffffff"
    signal clicked()

    radius: theme ? theme.buttonRadius : 6
    height: theme ? theme.buttonHeight : 42
    color: enabled ? fillColor : "#34383e"
    border.color: enabled ? Qt.lighter(fillColor, 1.25) : (theme ? theme.borderColor : "#3d444f")
    border.width: 1
    opacity: enabled ? 1.0 : 0.62

    Text {
        id: label
        anchors.centerIn: parent
        color: buttonRoot.textColor
        font.family: theme ? theme.fontFamily : "Arial"
        font.pixelSize: theme ? theme.bodyFontSize : 14
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
