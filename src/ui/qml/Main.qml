import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
  id: window
  visible: true
  width: 680
  height: 500
  color: "transparent"
  flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

  property bool settingsOpen: false

  onActiveChanged: {
    if (!window.active && !window.settingsOpen) {
      window.visible = false;
    }
  }

  onVisibleChanged: {
    if (window.visible) {
      searchBox.focusInput();
    }
  }

  // Center on screen horizontally and position in upper third
  Component.onCompleted: {
    x = (Screen.width - width) / 2;
    y = Math.max(80, (Screen.height - height) / 4);
    searchBox.focusInput();
  }

  // Floating Spotlight Card (Tasteful warm matte stone-gray like DigUp)
  Rectangle {
    id: card
    anchors.fill: parent
    radius: 14
    color: "#2d2c2a"
    border.color: Qt.rgba(255, 255, 255, 0.09)
    border.width: 1
    clip: true

    Item {
      anchors.fill: parent
      anchors.margins: 12

      // 1. Search Box
      SearchBox {
        id: searchBox
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        onSettingsClicked: {
          window.settingsOpen = !window.settingsOpen;
        }
        onUpPressed: {
          if (!window.settingsOpen) {
            resultsList.selectPrevious();
          }
        }
        onDownPressed: {
          if (!window.settingsOpen) {
            resultsList.selectNext();
          }
        }
        onReturnPressed: {
          if (!window.settingsOpen) {
            resultsList.activateCurrent();
          }
        }
        onEscapePressed: {
          handleEscape();
        }
      }

      // Top Hairline Divider
      Rectangle {
        id: topDivider
        anchors.top: searchBox.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Qt.rgba(255, 255, 255, 0.07)
        visible: !window.settingsOpen
      }

      // 2. Status Bar at Bottom
      StatusBar {
        id: statusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !window.settingsOpen
      }

      // 3. Central Content Area
      Item {
        id: centralArea
        anchors.top: window.settingsOpen ? searchBox.bottom : topDivider.bottom
        anchors.topMargin: 8
        anchors.bottom: window.settingsOpen ? parent.bottom : statusBar.top
        anchors.bottomMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right

        // Settings Sheet
        SettingsSheet {
          id: settingsSheet
          anchors.fill: parent
          visible: window.settingsOpen
          onCloseRequested: {
            window.settingsOpen = false;
            searchBox.focusInput();
          }
        }

        // Onboarding (First Run)
        OnboardingView {
          anchors.fill: parent
          visible: !window.settingsOpen && searchController.totalFiles === 0 && searchBox.text.trim().length === 0
          onAddFoldersClicked: {
            window.settingsOpen = true;
          }
        }

        // Default Empty Query View
        Item {
          anchors.fill: parent
          visible: !window.settingsOpen && searchController.totalFiles > 0 && searchBox.text.trim().length === 0

          Column {
            anchors.centerIn: parent
            spacing: 8

            Icon {
              anchors.horizontalCenter: parent.horizontalCenter
              name: "search"
              size: 24
              color: "#5c5955"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Search Files & Documents"
              font.pixelSize: 14
              font.weight: Font.DemiBold
              color: "#f5f5f5"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Search naturally across all your code, documents, and media."
              font.pixelSize: 12
              color: "#a8a6a3"
              horizontalAlignment: Text.AlignHCenter
              wrapMode: Text.WordWrap
              width: Math.min(420, parent.width - 40)
            }
          }
        }

        // No Results View
        Item {
          anchors.fill: parent
          visible: !window.settingsOpen && searchBox.text.trim().length > 0 && searchController.resultsModel.rowCount() === 0

          Column {
            anchors.centerIn: parent
            spacing: 8

            Icon {
              anchors.horizontalCenter: parent.horizontalCenter
              name: "folder"
              size: 24
              color: "#5c5955"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "No Results Found"
              font.pixelSize: 14
              font.weight: Font.DemiBold
              color: "#f5f5f5"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Try broader keywords or verify indexed folders in settings."
              font.pixelSize: 12
              color: "#a8a6a3"
              horizontalAlignment: Text.AlignHCenter
              wrapMode: Text.WordWrap
              width: Math.min(380, parent.width - 40)
            }
          }
        }

        // Results List
        ResultsList {
          id: resultsList
          anchors.fill: parent
          visible: !window.settingsOpen && searchBox.text.trim().length > 0 && searchController.resultsModel.rowCount() > 0
        }
      }
    }
  }

  function handleEscape() {
    if (window.settingsOpen) {
      window.settingsOpen = false;
      searchBox.focusInput();
    } else if (searchBox.text.length > 0) {
      searchBox.clearText();
    } else {
      window.visible = false;
    }
  }

  Connections {
    target: searchController
    function onFileOpened() {
      window.visible = false;
    }
  }

  Shortcut {
    sequence: "Escape"
    onActivated: {
      handleEscape();
    }
  }
}
