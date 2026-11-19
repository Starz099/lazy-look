import QtQuick
import QtQuick.Controls

Item {
  id: root
  height: 48

  property alias text: input.text
  signal settingsClicked()
  signal upPressed()
  signal downPressed()
  signal returnPressed()
  signal escapePressed()

  function focusInput() {
    input.forceActiveFocus();
  }

  function clearText() {
    input.text = "";
    searchController.search("");
    input.forceActiveFocus();
  }

  Rectangle {
    anchors.fill: parent
    radius: 10
    color: Qt.rgba(1, 1, 1, 0.05)
    border.color: input.activeFocus ? "#3584e4" : Qt.rgba(1, 1, 1, 0.12)
    border.width: input.activeFocus ? 2 : 1

    Behavior on border.color {
      ColorAnimation { duration: 120 }
    }

    Row {
      anchors.fill: parent
      anchors.leftMargin: 12
      anchors.rightMargin: 8
      spacing: 10

      // Search Icon
      Text {
        anchors.verticalCenter: parent.verticalCenter
        text: "🔍"
        font.pixelSize: 14
        opacity: 0.6
      }

      // Main Text Input
      TextInput {
        id: input
        width: parent.width - 96
        anchors.verticalCenter: parent.verticalCenter
        color: "#ffffff"
        font.pixelSize: 16
        font.weight: Font.Normal
        selectByMouse: true
        selectionColor: "#3584e4"
        selectedTextColor: "#ffffff"
        clip: true

        Text {
          anchors.fill: parent
          anchors.verticalCenter: parent.verticalCenter
          text: "Search files, documents, code..."
          color: "#77767b"
          font.pixelSize: 16
          visible: input.text.length === 0 && !input.inputMethodComposing
        }

        onTextChanged: {
          searchController.search(input.text);
        }

        Keys.onPressed: (event) => {
          if (event.key === Qt.Key_Down) {
            root.downPressed();
            event.accepted = true;
          } else if (event.key === Qt.Key_Up) {
            root.upPressed();
            event.accepted = true;
          } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.returnPressed();
            event.accepted = true;
          } else if (event.key === Qt.Key_Escape) {
            root.escapePressed();
            event.accepted = true;
          }
        }
      }

      // Clear Button (visible if text entered)
      Rectangle {
        width: 24
        height: 24
        radius: 12
        anchors.verticalCenter: parent.verticalCenter
        color: clearMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08)
        visible: input.text.length > 0

        Text {
          anchors.centerIn: parent
          text: "✕"
          font.pixelSize: 11
          color: "#deddda"
        }

        MouseArea {
          id: clearMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            root.clearText();
          }
        }
      }

      // Settings Button
      Rectangle {
        width: 32
        height: 32
        radius: 6
        anchors.verticalCenter: parent.verticalCenter
        color: settingsMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : "transparent"

        Text {
          anchors.centerIn: parent
          text: "⚙"
          font.pixelSize: 16
          color: settingsMouse.containsMouse ? "#ffffff" : "#9a9996"
        }

        ToolTip.visible: settingsMouse.containsMouse
        ToolTip.text: "Manage Indexed Folders (Ctrl+,)"
        ToolTip.delay: 400

        MouseArea {
          id: settingsMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            root.settingsClicked();
          }
        }
      }
    }
  }
}
