import QtQuick
import QtQuick.Controls

Item {
  id: root
  signal addFoldersClicked()

  Column {
    anchors.centerIn: parent
    spacing: 12
    width: parent.width - 64

    Text {
      anchors.horizontalCenter: parent.horizontalCenter
      text: "⚡"
      font.pixelSize: 36
    }

    Text {
      anchors.horizontalCenter: parent.horizontalCenter
      text: "Welcome to LazyFinder"
      font.pixelSize: 18
      font.bold: true
      color: "#ffffff"
    }

    Text {
      anchors.horizontalCenter: parent.horizontalCenter
      text: "Instant semantic and keyword search across your local files.\nAdd folders in Settings to start indexing."
      font.pixelSize: 13
      color: "#9a9996"
      horizontalAlignment: Text.AlignHCenter
      lineHeight: 1.3
    }

    Item {
      width: 1
      height: 4
    }

    Rectangle {
      anchors.horizontalCenter: parent.horizontalCenter
      height: 36
      width: btnText.implicitWidth + 32
      radius: 18
      color: btnMouse.containsMouse ? "#4793f0" : "#3584e4"

      Text {
        id: btnText
        anchors.centerIn: parent
        text: "📁 Setup Indexed Folders"
        font.pixelSize: 13
        font.bold: true
        color: "#ffffff"
      }

      MouseArea {
        id: btnMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
          root.addFoldersClicked();
        }
      }
    }
  }
}
