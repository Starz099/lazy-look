import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
  id: window
  visible: true

  readonly property int defaultWidth: 680
  readonly property int defaultHeight: 500
  readonly property int cardRadius: 14
  readonly property int cardMargin: 12

  width: defaultWidth
  height: defaultHeight
  color: "transparent"
  flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

  property bool settingsOpen: false

  onActiveChanged: {
    if (!window.active && !window.settingsOpen && !resultsList.menuOpen) {
      window.visible = false;
    } else if (window.active && !window.settingsOpen) {
      searchBox.focusInput();
    }
  }

  onVisibleChanged: {
    if (window.visible) {
      searchBox.focusInput();
    }
  }

  Component.onCompleted: {
    x = (Screen.width - width) / 2;
    y = Math.max(80, (Screen.height - height) / 4);
    searchBox.focusInput();
  }

  Rectangle {
    id: card
    anchors.fill: parent
    radius: window.cardRadius
    color: "#0b0d0a"
    border.color: Qt.rgba(255, 255, 255, 0.10)
    border.width: 1
    clip: true

    Item {
      anchors.fill: parent
      anchors.margins: window.cardMargin

      // Search Box
      SearchBox {
        id: searchBox
        objectName: "searchBox"
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
        onRevealRequested: {
          if (!window.settingsOpen) {
            resultsList.revealCurrent();
          }
        }
        onCopyPathRequested: {
          if (!window.settingsOpen) {
            resultsList.copyCurrent();
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

      // Status Bar at Bottom
      StatusBar {
        id: statusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !window.settingsOpen
      }

      // Central Content Area
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
              color: "#7f857b"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Search Files & Documents"
              font.pixelSize: 14
              font.weight: Font.DemiBold
              color: "#f3f5ef"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Search naturally across all your code, documents, and media."
              font.pixelSize: 12
              color: "#a0a59b"
              horizontalAlignment: Text.AlignHCenter
              wrapMode: Text.WordWrap
              width: Math.min(420, parent.width - 40)
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Try \"budget from last week\" or prefixes like doc:, code:, video:, image:"
              font.pixelSize: 11
              color: "#7f857b"
              horizontalAlignment: Text.AlignHCenter
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
              color: "#7f857b"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "No Results Found"
              font.pixelSize: 14
              font.weight: Font.DemiBold
              color: "#f3f5ef"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Try broader keywords or verify indexed folders in settings."
              font.pixelSize: 12
              color: "#a0a59b"
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
