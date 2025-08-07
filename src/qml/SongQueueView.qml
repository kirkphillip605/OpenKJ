import QtQuick 6.5
import QtQuick.Controls 6.5

TableView {
    id: queueView
    anchors.fill: parent
    clip: true
    model: songQueueModel

    delegate: Item {
        implicitWidth: 120
        implicitHeight: 24
        Text {
            anchors.centerIn: parent
            text: model.display
        }
    }
}
