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

    Rectangle {
      anchors.horizontalCenter: parent.horizontalCenter
      height: 24
      width: modelBadgeText.implicitWidth + 16
      radius: 12
      visible: searchController.isModelInstalled
      color: Qt.rgba(0.18, 0.76, 0.49, 0.15)
      border.color: "#2ec27e"
      border.width: 1

      Text {
        id: modelBadgeText
        anchors.centerIn: parent
        text: "AI Semantic Search Ready"
        font.pixelSize: 11
        color: "#2ec27e"
      }
    }

    // Interactive Download Card when model is not installed
    Column {
      anchors.horizontalCenter: parent.horizontalCenter
      spacing: 8
      visible: !searchController.isModelInstalled
      width: Math.min(360, parent.width)

      Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        height: 32
        width: dlBtnText.implicitWidth + 24
        radius: 16
        color: dlMouse.containsMouse ? "#3584e4" : Qt.rgba(0.21, 0.52, 0.89, 0.2)
        border.color: "#3584e4"
        border.width: 1
        visible: !searchController.isDownloadingModel

        Text {
          id: dlBtnText
          anchors.centerIn: parent
          text: "⬇ Download AI Model (310 MB)"
          font.pixelSize: 12
          font.bold: true
          color: "#ffffff"
        }

        MouseArea {
          id: dlMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            searchController.startModelDownload();
          }
        }
      }

      Column {
        width: parent.width
        spacing: 6
        visible: searchController.isDownloadingModel

        Rectangle {
          width: parent.width
          height: 6
          radius: 3
          color: Qt.rgba(1, 1, 1, 0.1)

          Rectangle {
            height: parent.height
            radius: 3
            width: parent.width * (Math.max(0, Math.min(100, searchController.modelDownloadProgress)) / 100.0)
            color: "#3584e4"
          }
        }

        Item {
          width: parent.width
          height: 16

          Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: searchController.modelDownloadStatus
            font.pixelSize: 10
            color: "#deddda"
          }

          Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "Cancel"
            font.pixelSize: 10
            color: cancelDlMouse.containsMouse ? "#f66151" : "#9a9996"

            MouseArea {
              id: cancelDlMouse
              anchors.fill: parent
              hoverEnabled: true
              cursorShape: Qt.PointingHandCursor
              onClicked: {
                searchController.cancelModelDownload();
              }
            }
          }
        }
      }
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
