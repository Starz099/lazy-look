import QtQuick
import QtQuick.Controls

Rectangle {
  id: root
  radius: 14
  color: "#242424"
  border.color: Qt.rgba(1, 1, 1, 0.12)
  border.width: 1

  signal closeRequested()

  Item {
    anchors.fill: parent
    anchors.margins: 16

    // Top Header (Fixed)
    Item {
      id: headerItem
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      height: 32

      Text {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: "⚙️ Preferences & Folders"
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
      id: headerSep
      anchors.top: headerItem.bottom
      anchors.topMargin: 8
      anchors.left: parent.left
      anchors.right: parent.right
      height: 1
      color: Qt.rgba(1, 1, 1, 0.08)
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
        policy: ScrollBar.AsNeeded
      }

      Column {
        id: contentCol
        width: parent.width
        spacing: 12

        // ==========================================
        // Section 1: Watched Folders
        // ==========================================
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
            height: 26
            width: addBtnText.implicitWidth + 18
            radius: 13
            color: addMouse.containsMouse ? "#4793f0" : "#3584e4"

            Text {
              id: addBtnText
              anchors.centerIn: parent
              text: "+ Add Folder..."
              font.pixelSize: 11
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
          height: 110
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
            spacing: 3

            delegate: Rectangle {
              id: rowDelegate
              required property string modelData
              required property int index
              width: dirList.width
              height: 32
              radius: 6
              color: rowHover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

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
                  spacing: 6

                  Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "📁"
                    font.pixelSize: 12
                  }

                  Text {
                    width: parent.width - 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData
                    font.pixelSize: 11
                    color: "#ffffff"
                    elide: Text.ElideMiddle
                  }
                }

                Rectangle {
                  id: delBtn
                  anchors.right: parent.right
                  anchors.verticalCenter: parent.verticalCenter
                  width: 24
                  height: 24
                  radius: 5
                  color: delMouse.containsMouse ? Qt.rgba(0.88, 0.11, 0.14, 0.4) : Qt.rgba(1, 1, 1, 0.06)
                  border.color: delMouse.containsMouse ? "#e01b24" : Qt.rgba(1, 1, 1, 0.1)
                  border.width: 1

                  Text {
                    anchors.centerIn: parent
                    text: "🗑"
                    font.pixelSize: 11
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
              text: "No directories added yet. Click '+ Add Folder...' above."
              font.pixelSize: 11
              color: "#77767b"
              visible: dirList.count === 0
            }
          }
        }

        Rectangle {
          width: parent.width
          height: 1
          color: Qt.rgba(1, 1, 1, 0.08)
        }

        // ==========================================
        // Section 2: Database Status & Manual Sync
        // ==========================================
        Item {
          width: parent.width
          height: 32

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Database Status"
              font.pixelSize: 12
              font.bold: true
              color: "#deddda"
            }

            Text {
              text: searchController.totalFiles + " files indexed (" + searchController.totalChunks + " chunks)"
              font.pixelSize: 11
              color: "#9a9996"
            }
          }

          Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 26
            width: syncBtnText.implicitWidth + 20
            radius: 5
            color: {
              if (searchController.isIndexing) {
                return syncMouse.containsMouse ? "#c01c28" : "#a51d2d";
              }
              return syncMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.1);
            }

            Row {
              anchors.centerIn: parent
              spacing: 5

              Text {
                anchors.verticalCenter: parent.verticalCenter
                text: searchController.isIndexing ? "⏹" : "🔄"
                font.pixelSize: 10
              }

              Text {
                id: syncBtnText
                anchors.verticalCenter: parent.verticalCenter
                text: searchController.isIndexing ? "Stop Sync" : "Sync Now"
                font.pixelSize: 11
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

        Text {
          width: parent.width
          text: searchController.indexingStatus
          font.pixelSize: 11
          color: searchController.isIndexing ? "#3584e4" : "#2ec27e"
          visible: searchController.indexingStatus.length > 0
          elide: Text.ElideRight
        }

        Rectangle {
          width: parent.width
          height: 1
          color: Qt.rgba(1, 1, 1, 0.08)
        }

        // ==========================================
        // Section 3: AI Engine & Models
        // ==========================================
        Text {
          text: "AI Neural Engine"
          font.pixelSize: 12
          font.bold: true
          color: "#deddda"
        }

        Item {
          width: parent.width
          height: 34

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Embedding Weights"
              font.pixelSize: 12
              color: "#ffffff"
            }

            Text {
              text: searchController.isModelInstalled ? "EmbeddingGemma-2 ready (768d vector space)" : "Model not found (run scripts/setup_models.sh)"
              font.pixelSize: 10
              color: searchController.isModelInstalled ? "#2ec27e" : "#f66151"
            }
          }

          Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle {
              height: 26
              width: dlModelBtnText.implicitWidth + 18
              radius: 5
              visible: !searchController.isModelInstalled && !searchController.isDownloadingModel
              color: dlModelMouse.containsMouse ? "#4793f0" : "#3584e4"

              Text {
                id: dlModelBtnText
                anchors.centerIn: parent
                text: "⬇ Download Model"
                font.pixelSize: 11
                font.bold: true
                color: "#ffffff"
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
              height: 26
              width: modelDirBtnText.implicitWidth + 18
              radius: 5
              color: modelDirMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.1)

              Text {
                id: modelDirBtnText
                anchors.centerIn: parent
                text: "Models Folder"
                font.pixelSize: 11
                color: "#ffffff"
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
              color: cancelDlSettingsMouse.containsMouse ? "#f66151" : "#9a9996"

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
          color: Qt.rgba(1, 1, 1, 0.08)
        }

        // ==========================================
        // Section 4: System & Preferences
        // ==========================================
        Text {
          text: "System & Integration"
          font.pixelSize: 12
          font.bold: true
          color: "#deddda"
        }

        // Global Shortcut Row
        Item {
          width: parent.width
          height: 34

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Global Shortcut"
              font.pixelSize: 12
              color: "#ffffff"
            }

            Text {
              text: "GNOME keybinding to toggle LazyFinder"
              font.pixelSize: 10
              color: "#9a9996"
            }
          }

          Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle {
              width: 150
              height: 26
              radius: 5
              color: Qt.rgba(0, 0, 0, 0.35)
              border.color: shortcutInput.activeFocus ? "#3584e4" : Qt.rgba(1, 1, 1, 0.15)
              border.width: 1

              TextInput {
                id: shortcutInput
                anchors.fill: parent
                anchors.margins: 4
                text: searchController.globalShortcut
                font.pixelSize: 11
                color: "#ffffff"
                verticalAlignment: TextInput.AlignVCenter
                horizontalAlignment: TextInput.AlignHCenter
                selectByMouse: true
                onAccepted: {
                  searchController.setGlobalShortcut(shortcutInput.text);
                }
              }
            }

            Rectangle {
              width: 50
              height: 26
              radius: 5
              color: saveScMouse.containsMouse ? "#4793f0" : "#3584e4"

              Text {
                anchors.centerIn: parent
                text: "Save"
                font.pixelSize: 11
                font.bold: true
                color: "#ffffff"
              }

              MouseArea {
                id: saveScMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                  searchController.setGlobalShortcut(shortcutInput.text);
                }
              }
            }
          }
        }

        // Auto-Sync Watcher Row
        Item {
          width: parent.width
          height: 34

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Real-Time Watcher (inotify)"
              font.pixelSize: 12
              color: "#ffffff"
            }

            Text {
              text: "Instantly re-index modified or newly created files"
              font.pixelSize: 10
              color: "#9a9996"
            }
          }

          Rectangle {
            id: autoSyncSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 22
            radius: 11
            color: searchController.autoSync ? "#3584e4" : Qt.rgba(1, 1, 1, 0.15)
            border.color: searchController.autoSync ? "#1c71d8" : Qt.rgba(1, 1, 1, 0.25)
            border.width: 1
            Behavior on color { ColorAnimation { duration: 150 } }

            Rectangle {
              width: 16
              height: 16
              radius: 8
              anchors.verticalCenter: parent.verticalCenter
              x: searchController.autoSync ? parent.width - width - 3 : 3
              color: "#ffffff"
              Behavior on x { NumberAnimation { duration: 150 } }
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
          height: 34

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Launch on Startup"
              font.pixelSize: 12
              color: "#ffffff"
            }

            Text {
              text: "Start in background on login (hotkey is only active when app is running)"
              font.pixelSize: 10
              color: "#9a9996"
            }
          }

          Rectangle {
            id: startupSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 22
            radius: 11
            color: searchController.launchOnStartup ? "#3584e4" : Qt.rgba(1, 1, 1, 0.15)
            border.color: searchController.launchOnStartup ? "#1c71d8" : Qt.rgba(1, 1, 1, 0.25)
            border.width: 1
            Behavior on color { ColorAnimation { duration: 150 } }

            Rectangle {
              width: 16
              height: 16
              radius: 8
              anchors.verticalCenter: parent.verticalCenter
              x: searchController.launchOnStartup ? parent.width - width - 3 : 3
              color: "#ffffff"
              Behavior on x { NumberAnimation { duration: 150 } }
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
          height: 34

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Memory Saver (Auto-Sleep)"
              font.pixelSize: 12
              color: "#ffffff"
            }

            Text {
              text: "Unload AI model from VRAM/RAM after 5m of inactivity"
              font.pixelSize: 10
              color: "#9a9996"
            }
          }

          Rectangle {
            id: memorySwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 22
            radius: 11
            color: searchController.memorySaver ? "#3584e4" : Qt.rgba(1, 1, 1, 0.15)
            border.color: searchController.memorySaver ? "#1c71d8" : Qt.rgba(1, 1, 1, 0.25)
            border.width: 1
            Behavior on color { ColorAnimation { duration: 150 } }

            Rectangle {
              width: 16
              height: 16
              radius: 8
              anchors.verticalCenter: parent.verticalCenter
              x: searchController.memorySaver ? parent.width - width - 3 : 3
              color: "#ffffff"
              Behavior on x { NumberAnimation { duration: 150 } }
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
          color: Qt.rgba(1, 1, 1, 0.08)
        }

        // Quit Button Row
        Item {
          width: parent.width
          height: 34

          Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
              text: "Quit Application"
              font.pixelSize: 12
              color: "#ffffff"
            }

            Text {
              text: "Close LazyFinder and release the global shortcut"
              font.pixelSize: 10
              color: "#9a9996"
            }
          }

          Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 26
            width: quitBtnText.implicitWidth + 20
            radius: 5
            color: quitMouse.containsMouse ? "#c01c28" : Qt.rgba(0.88, 0.11, 0.14, 0.15)
            border.color: "#c01c28"
            border.width: 1

            Text {
              id: quitBtnText
              anchors.centerIn: parent
              text: "Quit LazyFinder"
              font.pixelSize: 11
              font.bold: true
              color: quitMouse.containsMouse ? "#ffffff" : "#f66151"
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
