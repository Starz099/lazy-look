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
  signal revealRequested()
  signal copyPathRequested()
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
    color: "#121511"
    border.color: input.activeFocus ? Qt.rgba(200/255, 255/255, 61/255, 0.55) : Qt.rgba(255, 255, 255, 0.08)
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
      color: (input.text.length > 0 || input.activeFocus) ? "#c8ff3d" : "#7f857b"

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
          color: clearMouse.containsMouse ? "#f3f5ef" : "#a0a59b"
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
          color: settingsMouse.containsMouse ? "#f3f5ef" : "#a0a59b"
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
      color: "#f3f5ef"
      font.pixelSize: 15
      font.weight: Font.Normal
      selectByMouse: true
      selectionColor: "#c8ff3d"
      selectedTextColor: "#080a06"
      clip: true

      Text {
        anchors.fill: parent
        anchors.verticalCenter: parent.verticalCenter
        text: "Type to search"
        color: "#7f857b"
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
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
          root.revealRequested();
          event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
          root.returnPressed();
          event.accepted = true;
        } else if (event.key === Qt.Key_C && (event.modifiers & Qt.ControlModifier) && ((event.modifiers & Qt.ShiftModifier) || input.selectedText.length === 0)) {
          root.copyPathRequested();
          event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
          root.escapePressed();
          event.accepted = true;
        }
      }
    }
  }
}
