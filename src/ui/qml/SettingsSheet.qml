import QtQuick
import QtQuick.Controls

Rectangle {
  id: root
  radius: 12
  color: "#252423"
  border.color: Qt.rgba(255, 255, 255, 0.08)
  border.width: 1

  signal closeRequested()

  Item {
    anchors.fill: parent
    anchors.margins: 14

    // Header
    Item {
      id: headerItem
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      height: 28

      Row {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Icon {
          anchors.verticalCenter: parent.verticalCenter
          name: "settings"
          size: 15
          color: "#f5f5f5"
        }

        Text {
          anchors.verticalCenter: parent.verticalCenter
          text: "Preferences"
          font.pixelSize: 13
          font.weight: Font.DemiBold
          color: "#f5f5f5"
        }
      }

      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 4
        color: closeMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

        Icon {
          anchors.centerIn: parent
          name: "close"
          size: 12
          color: closeMouse.containsMouse ? "#f5f5f5" : "#a8a6a3"
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
      id: headerSep
      anchors.top: headerItem.bottom
      anchors.topMargin: 8
      anchors.left: parent.left
      anchors.right: parent.right
      height: 1
      color: Qt.rgba(255, 255, 255, 0.07)
    }

    // Scrollable Settings Content
    Flickable {
      id: scrollArea
      anchors.top: headerSep.bottom
      anchors.topMargin: 10
      anchors.bottom: parent.bottom
      anchors.left: parent.left
      anchors.right: parent.right
      contentWidth: width
      contentHeight: contentCol.implicitHeight + 20
      clip: true
      flickableDirection: Flickable.VerticalFlick
      ScrollBar.vertical: ScrollBar {
        id: settingsScroll
        policy: ScrollBar.AsNeeded
        width: 4
        contentItem: Rectangle {
          implicitWidth: 4
          radius: 2
          color: settingsScroll.pressed ? Qt.rgba(255, 255, 255, 0.4) : (settingsScroll.hovered ? Qt.rgba(255, 255, 255, 0.25) : Qt.rgba(255, 255, 255, 0.16))
          Behavior on color { ColorAnimation { duration: 100 } }
        }
        background: Item {}
      }

      Column {
        id: contentCol
        width: parent.width - 14
        spacing: 14

        // Watched Folders
        Item {
          width: parent.width
          height: 26

          Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "Indexed Folders"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: "#f5f5f5"
          }

          Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 24
            width: addBtnRow.implicitWidth + 16
            radius: 5
            color: addMouse.containsMouse ? "#1d4ed8" : "#2563eb"

            Row {
              id: addBtnRow
              anchors.centerIn: parent
              spacing: 5

              Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "folder"
                size: 11
                color: "#ffffff"
              }

              Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Add Folder"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: "#ffffff"
              }
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

        // Watched Directories List
        Rectangle {
          width: parent.width
          height: Math.min(130, Math.max(64, dirList.count * 34 + 8))
          radius: 7
          color: "#1e1d1c"
          border.color: Qt.rgba(255, 255, 255, 0.06)
          border.width: 1
          clip: true

          ListView {
            id: dirList
            anchors.fill: parent
            anchors.margins: 4
            model: searchController.watchedDirectories
            spacing: 2

            delegate: Rectangle {
              id: rowDelegate
              required property string modelData
              required property int index
              width: dirList.width
              height: 30
              radius: 5
              color: rowHover.hovered ? Qt.rgba(255, 255, 255, 0.04) : "transparent"

              HoverHandler {
                id: rowHover
              }

              Item {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 6

                Row {
                  anchors.left: parent.left
                  anchors.right: delBtn.left
                  anchors.rightMargin: 8
                  anchors.verticalCenter: parent.verticalCenter
                  spacing: 7

                  Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "folder"
                    size: 13
                    color: "#8c8a87"
                  }

                  Text {
                    width: parent.width - 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData
                    font.pixelSize: 11
                    color: "#f5f5f5"
                    elide: Text.ElideMiddle
                  }
                }

                Rectangle {
                  id: delBtn
                  anchors.right: parent.right
                  anchors.verticalCenter: parent.verticalCenter
                  width: 22
                  height: 22
                  radius: 4
                  color: delMouse.containsMouse ? Qt.rgba(239, 68, 68, 0.15) : "transparent"

                  Icon {
                    anchors.centerIn: parent
                    name: "trash"
                    size: 11
                    color: delMouse.containsMouse ? "#ef4444" : "#8c8a87"
                  }

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
              text: "No indexed folders added yet."
              font.pixelSize: 11
              color: "#6c6966"
              visible: dirList.count === 0
            }
          }
        }

        Rectangle {
          width: parent.width
          height: 1
          color: Qt.rgba(255, 255, 255, 0.07)
        }

        // Search Index and Manual Sync
        Item {
          width: parent.width
          height: 32

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Search Index"
              font.pixelSize: 12
              font.weight: Font.DemiBold
              color: "#f5f5f5"
            }

            Text {
              text: searchController.totalFiles + " files indexed (" + searchController.totalChunks + " chunks)"
              font.pixelSize: 11
              color: "#8c8a87"
            }
          }

          Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 24
            width: syncBtnRow.implicitWidth + 16
            radius: 5
            color: {
              if (searchController.isIndexing) {
                return syncMouse.containsMouse ? "#dc2626" : "#ef4444";
              }
              return syncMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(255, 255, 255, 0.06);
            }

            Row {
              id: syncBtnRow
              anchors.centerIn: parent
              spacing: 5

              Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: searchController.isIndexing ? "stop" : "sync"
                size: 11
                color: "#ffffff"
              }

              Text {
                anchors.verticalCenter: parent.verticalCenter
                text: searchController.isIndexing ? "Stop Sync" : "Sync Now"
                font.pixelSize: 11
                font.weight: Font.Medium
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

        Text {
          width: parent.width
          text: searchController.indexingStatus
          font.pixelSize: 11
          color: searchController.isIndexing ? "#60a5fa" : "#10b981"
          visible: searchController.indexingStatus.length > 0
          elide: Text.ElideRight
        }

        Rectangle {
          width: parent.width
          height: 1
          color: Qt.rgba(255, 255, 255, 0.07)
        }

        // AI Neural Weights
        Item {
          width: parent.width
          height: 32

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "AI Neural Weights"
              font.pixelSize: 12
              font.weight: Font.DemiBold
              color: "#f5f5f5"
            }

            Text {
              text: searchController.isModelInstalled ? "EmbeddingGemma-2 Q8_0 active (768d vectors)" : "Weights not found (run setup script or download)"
              font.pixelSize: 11
              color: searchController.isModelInstalled ? "#10b981" : "#f59e0b"
            }
          }

          Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle {
              height: 24
              width: dlModelRow.implicitWidth + 14
              radius: 5
              visible: !searchController.isModelInstalled && !searchController.isDownloadingModel
              color: dlModelMouse.containsMouse ? "#1d4ed8" : "#2563eb"

              Row {
                id: dlModelRow
                anchors.centerIn: parent
                spacing: 4

                Icon {
                  anchors.verticalCenter: parent.verticalCenter
                  name: "download"
                  size: 11
                  color: "#ffffff"
                }

                Text {
                  anchors.verticalCenter: parent.verticalCenter
                  text: "Download"
                  font.pixelSize: 11
                  font.weight: Font.Medium
                  color: "#ffffff"
                }
              }

              MouseArea {
                id: dlModelMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                  searchController.startModelDownload();
                }
              }
            }

            Rectangle {
              height: 24
              width: modelDirRow.implicitWidth + 14
              radius: 5
              color: modelDirMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(255, 255, 255, 0.06)

              Row {
                id: modelDirRow
                anchors.centerIn: parent
                spacing: 4

                Icon {
                  anchors.verticalCenter: parent.verticalCenter
                  name: "folder"
                  size: 11
                  color: "#a8a6a3"
                }

                Text {
                  anchors.verticalCenter: parent.verticalCenter
                  text: "Models Folder"
                  font.pixelSize: 11
                  color: "#f5f5f5"
                }
              }

              MouseArea {
                id: modelDirMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                  searchController.openModelDirectory();
                }
              }
            }
          }
        }

        // Live Download Progress (Visible during download)
        Column {
          width: parent.width
          spacing: 4
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
              color: "#2563eb"
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
              color: "#a8a6a3"
            }

            Text {
              anchors.right: parent.right
              anchors.verticalCenter: parent.verticalCenter
              text: "Cancel"
              font.pixelSize: 10
              color: cancelDlSettingsMouse.containsMouse ? "#ef4444" : "#8c8a87"

              MouseArea {
                id: cancelDlSettingsMouse
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

        Rectangle {
          width: parent.width
          height: 1
          color: Qt.rgba(255, 255, 255, 0.07)
        }

        // System Preferences
        Text {
          text: "System Preferences"
          font.pixelSize: 12
          font.weight: Font.DemiBold
          color: "#f5f5f5"
        }

        // Auto-Sync Watcher Row
        Item {
          width: parent.width
          height: 32

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
              text: "Background Inotify Watcher"
              font.pixelSize: 12
              color: "#f5f5f5"
            }

            Text {
              text: "Auto re-index modified files when saved to disk"
              font.pixelSize: 10
              color: "#8c8a87"
            }
          }

          Rectangle {
            id: autoSyncSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 34
            height: 18
            radius: 9
            color: searchController.autoSync ? "#2563eb" : Qt.rgba(255, 255, 255, 0.12)
            Behavior on color { ColorAnimation { duration: 120 } }

            Rectangle {
              width: 14
              height: 14
              radius: 7
              anchors.verticalCenter: parent.verticalCenter
              x: searchController.autoSync ? parent.width - width - 2 : 2
              color: "#ffffff"
              Behavior on x { NumberAnimation { duration: 120 } }
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.PointingHandCursor
              onClicked: {
                searchController.setAutoSync(!searchController.autoSync);
              }
            }
          }
        }

        // Launch on Startup Row
        Item {
          width: parent.width
          height: 32

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
              text: "Launch on Login"
              font.pixelSize: 12
              color: "#f5f5f5"
            }

            Text {
              text: "Run daemon in background when logging in"
              font.pixelSize: 10
              color: "#8c8a87"
            }
          }

          Rectangle {
            id: startupSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 34
            height: 18
            radius: 9
            color: searchController.launchOnStartup ? "#2563eb" : Qt.rgba(255, 255, 255, 0.12)
            Behavior on color { ColorAnimation { duration: 120 } }

            Rectangle {
              width: 14
              height: 14
              radius: 7
              anchors.verticalCenter: parent.verticalCenter
              x: searchController.launchOnStartup ? parent.width - width - 2 : 2
              color: "#ffffff"
              Behavior on x { NumberAnimation { duration: 120 } }
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.PointingHandCursor
              onClicked: {
                searchController.setLaunchOnStartup(!searchController.launchOnStartup);
              }
            }
          }
        }

        // Memory Saver Row
        Item {
          width: parent.width
          height: 32

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
              text: "Memory Saver (Auto-Sleep)"
              font.pixelSize: 12
              color: "#f5f5f5"
            }

            Text {
              text: "Unload model from GPU VRAM after 5 minutes of idle"
              font.pixelSize: 10
              color: "#8c8a87"
            }
          }

          Rectangle {
            id: memorySwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 34
            height: 18
            radius: 9
            color: searchController.memorySaver ? "#2563eb" : Qt.rgba(255, 255, 255, 0.12)
            Behavior on color { ColorAnimation { duration: 120 } }

            Rectangle {
              width: 14
              height: 14
              radius: 7
              anchors.verticalCenter: parent.verticalCenter
              x: searchController.memorySaver ? parent.width - width - 2 : 2
              color: "#ffffff"
              Behavior on x { NumberAnimation { duration: 120 } }
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.PointingHandCursor
              onClicked: {
                searchController.setMemorySaver(!searchController.memorySaver);
              }
            }
          }
        }

        Rectangle {
          width: parent.width
          height: 1
          color: Qt.rgba(255, 255, 255, 0.07)
        }

        // Quit Button Row
        Item {
          width: parent.width
          height: 32

          Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "Exit Application"
            font.pixelSize: 12
            color: "#8c8a87"
          }

          Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 24
            width: quitBtnText.implicitWidth + 16
            radius: 5
            color: quitMouse.containsMouse ? Qt.rgba(239, 68, 68, 0.2) : Qt.rgba(239, 68, 68, 0.1)
            border.color: Qt.rgba(239, 68, 68, 0.3)
            border.width: 1

            Text {
              id: quitBtnText
              anchors.centerIn: parent
              text: "Quit LazyLook"
              font.pixelSize: 11
              font.weight: Font.Medium
              color: quitMouse.containsMouse ? "#ffffff" : "#ef4444"
            }

            MouseArea {
              id: quitMouse
              anchors.fill: parent
              hoverEnabled: true
              cursorShape: Qt.PointingHandCursor
              onClicked: {
                searchController.quitApp();
              }
            }
          }
        }
      }
    }
  }
}
