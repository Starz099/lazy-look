import QtQuick
import QtQuick.Controls

Rectangle {
  id: root
  radius: 14
  color: "#242424"
  border.color: Qt.rgba(1, 1, 1, 0.12)
  border.width: 1

  signal closeRequested()

  Column {
    anchors.fill: parent
    anchors.margins: 16
    spacing: 14

    // Top Header
    Item {
      width: parent.width
      height: 32

      Text {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: "📁 Indexed Folders & Settings"
        font.pixelSize: 15
        font.bold: true
        color: "#ffffff"
      }

      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        radius: 14
        color: closeMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.08)

        Text {
          anchors.centerIn: parent
          text: "✕"
          font.pixelSize: 12
          color: "#deddda"
        }

        MouseArea {
          id: closeMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            root.closeRequested();
          }
        }
      }
    }

    Rectangle {
      width: parent.width
      height: 1
      color: Qt.rgba(1, 1, 1, 0.08)
    }

    // Section 1: Watched Directories
    Item {
      width: parent.width
      height: 28

      Text {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: "Watched Directories"
        font.pixelSize: 13
        font.bold: true
        color: "#deddda"
      }

      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 28
        width: addBtnText.implicitWidth + 20
        radius: 14
        color: addMouse.containsMouse ? "#4793f0" : "#3584e4"

        Text {
          id: addBtnText
          anchors.centerIn: parent
          text: "+ Add Folder..."
          font.pixelSize: 12
          font.bold: true
          color: "#ffffff"
        }

        MouseArea {
          id: addMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            searchController.chooseFolderAndAdd();
          }
        }
      }
    }

    Text {
      width: parent.width
      text: "Files in these directories are indexed and searchable using natural language."
      font.pixelSize: 11
      color: "#9a9996"
      wrapMode: Text.WordWrap
    }

    // Watched Directories List
    Rectangle {
      width: parent.width
      height: 140
      radius: 8
      color: Qt.rgba(0, 0, 0, 0.25)
      border.color: Qt.rgba(1, 1, 1, 0.08)
      border.width: 1
      clip: true

      ListView {
        id: dirList
        anchors.fill: parent
        anchors.margins: 4
        model: searchController.watchedDirectories
        spacing: 4

        delegate: Rectangle {
          id: rowDelegate
          required property string modelData
          required property int index
          width: dirList.width
          height: 36
          radius: 6
          color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

          HoverHandler {
            id: rowHover
          }

          Item {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 8

            Row {
              anchors.left: parent.left
              anchors.right: delBtn.left
              anchors.rightMargin: 8
              anchors.verticalCenter: parent.verticalCenter
              spacing: 8

              Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "📁"
                font.pixelSize: 13
              }

              Text {
                width: parent.width - 28
                anchors.verticalCenter: parent.verticalCenter
                text: modelData
                font.pixelSize: 12
                color: "#ffffff"
                elide: Text.ElideMiddle
              }
            }

            Rectangle {
              id: delBtn
              z: 2
              anchors.right: parent.right
              anchors.verticalCenter: parent.verticalCenter
              width: 28
              height: 28
              radius: 6
              color: delMouse.containsMouse ? Qt.rgba(0.88, 0.11, 0.14, 0.4) : Qt.rgba(1, 1, 1, 0.06)
              border.color: delMouse.containsMouse ? "#e01b24" : Qt.rgba(1, 1, 1, 0.1)
              border.width: 1

              Text {
                anchors.centerIn: parent
                text: "🗑"
                font.pixelSize: 13
              }

              ToolTip.visible: delMouse.containsMouse
              ToolTip.text: "Remove from indexed folders"
              ToolTip.delay: 200

              MouseArea {
                id: delMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                  searchController.removeWatchedDirectory(rowDelegate.modelData);
                }
              }
            }
          }
        }

        Text {
          anchors.centerIn: parent
          text: "No directories added yet.\nClick '+ Add Folder...' above to get started."
          font.pixelSize: 12
          color: "#77767b"
          horizontalAlignment: Text.AlignHCenter
          visible: dirList.count === 0
        }
      }
    }

    Rectangle {
      width: parent.width
      height: 1
      color: Qt.rgba(1, 1, 1, 0.08)
    }

    // Section 2: Database Status & Manual Sync
    Item {
      width: parent.width
      height: 36

      Column {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
          text: "Database Status"
          font.pixelSize: 13
          font.bold: true
          color: "#deddda"
        }

        Text {
          text: searchController.totalFiles + " files indexed (" + searchController.totalChunks + " chunks)"
          font.pixelSize: 11
          color: "#9a9996"
        }
      }

      // Sync Now / Stop Button
      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 30
        width: syncBtnText.implicitWidth + 24
        radius: 6
        color: {
          if (searchController.isIndexing) {
            return syncMouse.containsMouse ? "#c01c28" : "#a51d2d";
          }
          return syncMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.1);
        }

        Row {
          anchors.centerIn: parent
          spacing: 6

          Text {
            anchors.verticalCenter: parent.verticalCenter
            text: searchController.isIndexing ? "⏹" : "🔄"
            font.pixelSize: 11
          }

          Text {
            id: syncBtnText
            anchors.verticalCenter: parent.verticalCenter
            text: searchController.isIndexing ? "Stop Sync" : "Sync Now"
            font.pixelSize: 12
            font.bold: true
            color: "#ffffff"
          }
        }

        MouseArea {
          id: syncMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            if (searchController.isIndexing) {
              searchController.cancelSync();
            } else {
              searchController.startSync();
            }
          }
        }
      }
    }

    // Status message during or after indexing
    Text {
      width: parent.width
      text: searchController.indexingStatus
      font.pixelSize: 11
      color: searchController.isIndexing ? "#3584e4" : "#2ec27e"
      visible: searchController.indexingStatus.length > 0
      elide: Text.ElideRight
    }
  }
}
