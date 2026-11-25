import QtQuick
import QtQuick.Controls

Item {
  id: root
  signal addFoldersClicked()

  Column {
    anchors.centerIn: parent
    spacing: 16
    width: Math.min(440, parent.width - 48)

    Column {
      anchors.horizontalCenter: parent.horizontalCenter
      spacing: 6
      width: parent.width

      Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Welcome to LazyLook"
        font.pixelSize: 18
        font.weight: Font.DemiBold
        color: "#f3f5ef"
      }

      Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Fast on-device semantic search for your documents, code, and media."
        font.pixelSize: 13
        color: "#a0a59b"
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        width: parent.width
      }

      Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: "Describe what you're looking for in natural language to find content across your files."
        font.pixelSize: 12
        color: "#7f857b"
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        width: parent.width
      }
    }

    // AI Model Download Section (When Missing)
    Column {
      anchors.horizontalCenter: parent.horizontalCenter
      spacing: 8
      visible: !searchController.isModelInstalled
      width: parent.width

      Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        height: 32
        width: dlBtnRow.implicitWidth + 24
        radius: 6
        color: dlMouse.containsMouse ? "#9ee82a" : "#c8ff3d"
        visible: !searchController.isDownloadingModel

        Row {
          id: dlBtnRow
          anchors.centerIn: parent
          spacing: 6

          Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: "download"
            size: 13
            color: "#080a06"
          }

          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Download AI Model (310 MB)"
            font.pixelSize: 12
            font.weight: Font.Medium
            color: "#080a06"
          }
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

      // Download Progress
      Column {
        width: parent.width
        spacing: 6
        visible: searchController.isDownloadingModel

        Rectangle {
          width: parent.width
          height: 4
          radius: 2
          color: Qt.rgba(255, 255, 255, 0.08)

          Rectangle {
            height: parent.height
            radius: 2
            width: parent.width * (Math.max(0, Math.min(100, searchController.modelDownloadProgress)) / 100.0)
            color: "#c8ff3d"
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
            color: "#a0a59b"
          }

          Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "Cancel"
            font.pixelSize: 10
            color: cancelDlMouse.containsMouse ? "#ef4444" : "#7f857b"

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

    // Subtle Link instead of Heavy Button
    Row {
      anchors.horizontalCenter: parent.horizontalCenter
      spacing: 5

      Text {
        anchors.verticalCenter: parent.verticalCenter
        text: "or"
        font.pixelSize: 12
        color: "#7f857b"
      }

      Text {
        id: linkText
        anchors.verticalCenter: parent.verticalCenter
        text: "choose folders to index"
        font.pixelSize: 12
        font.weight: Font.Medium
        color: linkMouse.containsMouse ? "#d8ff70" : "#c8ff3d"
        font.underline: linkMouse.containsMouse

        MouseArea {
          id: linkMouse
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
}
