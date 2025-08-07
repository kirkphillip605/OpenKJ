import QtQuick 2.15

Item {
    id: root
    property alias text: tickerText.text
    property alias font: tickerText.font
    property alias color: tickerText.color
    property int speed: 10000 // duration in ms for full scroll

    width: parent ? parent.width : 400
    height: tickerText.contentHeight
    clip: true

    Text {
        id: tickerText
        text: ""
        x: root.width
        anchors.verticalCenter: parent.verticalCenter
    }

    NumberAnimation {
        id: anim
        target: tickerText
        property: "x"
        from: root.width
        to: -tickerText.width
        duration: speed
        loops: Animation.Infinite
        running: root.visible && tickerText.width > root.width
    }
}
