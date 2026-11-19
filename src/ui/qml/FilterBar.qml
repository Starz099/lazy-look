import QtQuick
import QtQuick.Controls

Item {
  id: root
  height: 28

  readonly property var filters: [
    { id: "all", label: "All Files" },
    { id: "doc", label: "Documents" },
    { id: "code", label: "Code" },
    { id: "media", label: "Media" }
  ]

  Row {
    anchors.fill: parent
    spacing: 8

    Repeater {
      model: root.filters

      Rectangle {
        id: pill
        height: 26
        width: pillText.implicitWidth + 20
        radius: 13
        property bool isActive: searchController.currentFilter === modelData.id
        color: isActive ? "#3584e4" : (pillMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06))

        Behavior on color {
          ColorAnimation { duration: 100 }
        }

        Text {
          id: pillText
          anchors.centerIn: parent
          text: modelData.label
          font.pixelSize: 12
          font.weight: pill.isActive ? Font.DemiBold : Font.Normal
          color: pill.isActive ? "#ffffff" : "#c0bfbc"
        }

        MouseArea {
          id: pillMouse
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: {
            searchController.setCurrentFilter(modelData.id);
          }
        }
      }
    }
  }
}
