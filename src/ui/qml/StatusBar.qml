import QtQuick
import QtQuick.Controls

Item {
  id: root
  height: 28

  Row {
    anchors.left: parent.left
    anchors.verticalCenter: parent.verticalCenter
    spacing: 12
    visible: !searchController.isIndexing

    component KeyHint: Row {
      spacing: 4
      property alias key: keyLabel.text
      property alias desc: descLabel.text

      Rectangle {
        height: 18
        width: keyLabel.implicitWidth + 8
        radius: 4
        color: Qt.rgba(1, 1, 1, 0.1)
        anchors.verticalCenter: parent.verticalCenter

        Text {
          id: keyLabel
          anchors.centerIn: parent
          font.pixelSize: 10
          font.bold: true
          color: "#deddda"
        }
      }

      Text {
        id: descLabel
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: 11
        color: "#9a9996"
      }
    }

    KeyHint { key: "↵"; desc: "Open" }
    KeyHint { key: "Ctrl+↵"; desc: "Folder" }
    KeyHint { key: "Ctrl+C"; desc: "Copy" }
    KeyHint { key: "Ctrl+,"; desc: "Settings" }
    KeyHint { key: "Esc"; desc: "Close" }
  }

  // Indexing status indicator (visible while indexing)
  Row {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    spacing: 8
    visible: searchController.isIndexing

    BusyIndicator {
      id: spinner
      width: 16
      height: 16
      anchors.verticalCenter: parent.verticalCenter
      running: searchController.isIndexing
    }

    Text {
      anchors.verticalCenter: parent.verticalCenter
      text: searchController.indexingStatus
      font.pixelSize: 11
      color: "#3584e4"
      elide: Text.ElideRight
      width: parent.width - 24
    }
  }
}
