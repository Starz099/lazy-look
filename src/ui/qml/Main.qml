import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
  id: window
  visible: true
  width: 680
  height: 520
  color: "transparent"
  flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

  property bool settingsOpen: false

  // Center on screen horizontally and position in upper third
  Component.onCompleted: {
    x = (Screen.width - width) / 2;
    y = Math.max(80, (Screen.height - height) / 4);
    searchBox.focusInput();
  }

  // Floating Spotlight Card
  Rectangle {
    id: card
    anchors.fill: parent
    radius: 16
    color: "#1e1e1e"
    border.color: Qt.rgba(1, 1, 1, 0.14)
    border.width: 1
    clip: true

    Column {
      anchors.fill: parent
      anchors.margins: 14
      spacing: 10

      // 1. Search Box
      SearchBox {
        id: searchBox
        width: parent.width
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

      // 2. Filter Pills
      FilterBar {
        id: filterBar
        width: parent.width
        visible: !window.settingsOpen
      }

      Rectangle {
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
        visible: !window.settingsOpen
      }

      // 3. Central Content Area
      Item {
        id: centralArea
        width: parent.width
        height: parent.height - (window.settingsOpen ? 60 : 138)

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

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "🔎"
              font.pixelSize: 32
              opacity: 0.8
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Quick File Search"
              font.pixelSize: 16
              font.bold: true
              color: "#ffffff"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Search across " + searchController.totalFiles + " indexed files in your folders.\nPress Ctrl+, to manage folders."
              font.pixelSize: 12
              color: "#9a9996"
              horizontalAlignment: Text.AlignHCenter
              lineHeight: 1.3
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

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "📂"
              font.pixelSize: 32
              opacity: 0.6
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "No Files Found"
              font.pixelSize: 16
              font.bold: true
              color: "#ffffff"
            }

            Text {
              anchors.horizontalCenter: parent.horizontalCenter
              text: "Try broader keywords, switch filter pills, or verify indexed folders."
              font.pixelSize: 12
              color: "#9a9996"
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

      Rectangle {
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
        visible: !window.settingsOpen
      }

      // 4. Status Bar
      StatusBar {
        width: parent.width
        visible: !window.settingsOpen
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
      Qt.quit();
    }
  }

  // Global Key Shortcuts
  Shortcut {
    sequence: "Ctrl+,"
    onActivated: {
      window.settingsOpen = !window.settingsOpen;
    }
  }

  Shortcut {
    sequence: "Ctrl+Return"
    onActivated: {
      if (!window.settingsOpen) {
        resultsList.revealCurrent();
      }
    }
  }

  Shortcut {
    sequence: "Ctrl+C"
    onActivated: {
      if (!window.settingsOpen) {
        resultsList.copyCurrent();
      }
    }
  }

  Shortcut {
    sequence: "Escape"
    onActivated: {
      handleEscape();
    }
  }
}
