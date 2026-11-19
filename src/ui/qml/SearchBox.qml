import QtQuick
import QtQuick.Controls

Item {
  id: root
  height: 46

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
    id: boxContainer
    anchors.fill: parent
    radius: 9
    color: "#242322"
    border.color: input.activeFocus ? "#f59e0b" : Qt.rgba(255, 255, 255, 0.08)
    border.width: 1

    Behavior on border.color {
      ColorAnimation { duration: 120 }
    }

    // Leading Search Icon
    Icon {
      id: searchIcon
      anchors.left: parent.left
      anchors.leftMargin: 12
      anchors.verticalCenter: parent.verticalCenter
      name: "search"
      size: 16
      color: (input.text.length > 0 || input.activeFocus) ? "#f59e0b" : "#8c8a87"

      Behavior on color {
        ColorAnimation { duration: 120 }
      }
    }

    // Trailing Controls (Clear + Settings)
    Row {
      id: trailingControls
      anchors.right: parent.right
      anchors.rightMargin: 8
      anchors.verticalCenter: parent.verticalCenter
      spacing: 4

      // Clear button (shown when query is non-empty)
      Rectangle {
        id: clearBtn
        width: 24
        height: 24
        radius: 12
        anchors.verticalCenter: parent.verticalCenter
        visible: input.text.length > 0
        color: clearMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"

        Icon {
          anchors.centerIn: parent
          name: "close"
          size: 12
          color: clearMouse.containsMouse ? "#f5f5f5" : "#a8a6a3"
        }

        MouseArea {
          id: clearMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: root.clearText()
        }
      }

      // Settings button
      Rectangle {
        id: settingsBtn
        width: 28
        height: 28
        radius: 6
        anchors.verticalCenter: parent.verticalCenter
        color: settingsMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

        Icon {
          anchors.centerIn: parent
          name: "settings"
          size: 14
          color: settingsMouse.containsMouse ? "#f5f5f5" : "#a8a6a3"
        }

        MouseArea {
          id: settingsMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: root.settingsClicked()
        }
      }
    }

    // Search Query Input
    TextInput {
      id: input
      anchors.left: searchIcon.right
      anchors.leftMargin: 10
      anchors.right: trailingControls.left
      anchors.rightMargin: 8
      anchors.verticalCenter: parent.verticalCenter
      color: "#f5f5f5"
      font.pixelSize: 15
      font.weight: Font.Normal
      selectByMouse: true
      selectionColor: "#3f5675"
      selectedTextColor: "#ffffff"
      clip: true

      Text {
        anchors.fill: parent
        anchors.verticalCenter: parent.verticalCenter
        text: "Type to search"
        color: "#6c6966"
        font.pixelSize: 15
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
  }
}
