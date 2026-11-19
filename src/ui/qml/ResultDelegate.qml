import QtQuick
import QtQuick.Controls

Item {
  id: root
  width: ListView.view.width
  height: snippetText.text.length > 0 ? 62 : 44

  required property int index
  required property string filePath
  required property string fileName
  required property string directory
  required property string score
  required property string snippet
  required property string category
  required property string iconName

  property bool isSelected: ListView.isCurrentItem

  Rectangle {
    id: bg
    anchors.fill: parent
    anchors.leftMargin: 8
    anchors.rightMargin: 8
    anchors.topMargin: 2
    anchors.bottomMargin: 2
    radius: 8
    color: root.isSelected ? Qt.rgba(0.21, 0.52, 0.89, 0.28) : (mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
    border.color: root.isSelected ? Qt.rgba(0.21, 0.52, 0.89, 0.5) : "transparent"
    border.width: 1

    Behavior on color {
      ColorAnimation { duration: 80 }
    }

    Row {
      anchors.fill: parent
      anchors.margins: 8
      spacing: 12

      // Category / Type Badge
      Rectangle {
        width: 32
        height: 32
        radius: 6
        anchors.verticalCenter: parent.verticalCenter
        color: {
          switch (root.category) {
            case "code": return Qt.rgba(0.21, 0.52, 0.89, 0.2);
            case "doc": return Qt.rgba(0.9, 0.38, 0.0, 0.2);
            case "media": return Qt.rgba(0.57, 0.25, 0.67, 0.2);
            default: return Qt.rgba(0.47, 0.46, 0.48, 0.2);
          }
        }

        Text {
          anchors.centerIn: parent
          text: {
            switch (root.iconName) {
              case "code": return "</>";
              case "pdf": return "PDF";
              case "markdown": return "MD";
              case "document": return "DOC";
              case "image": return "IMG";
              case "video": return "VID";
              case "audio": return "AUD";
              default: return "FILE";
            }
          }
          font.pixelSize: 10
          font.bold: true
          color: {
            switch (root.category) {
              case "code": return "#78aeed";
              case "doc": return "#ff7800";
              case "media": return "#c061cb";
              default: return "#deddda";
            }
          }
        }
      }

      // Details Column
      Column {
        width: parent.width - 44
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        // Top Container: Title + Path + Score
        Item {
          width: parent.width
          height: 18

          Row {
            anchors.left: parent.left
            anchors.right: scorePill.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Text {
              id: fileNameText
              text: root.fileName
              font.pixelSize: 13
              font.bold: true
              color: "#ffffff"
              elide: Text.ElideRight
              maximumLineCount: 1
            }

            Text {
              width: Math.max(50, parent.width - fileNameText.implicitWidth - 8)
              text: root.directory
              font.pixelSize: 11
              color: "#9a9996"
              elide: Text.ElideMiddle
              maximumLineCount: 1
            }
          }

          Rectangle {
            id: scorePill
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: scoreText.implicitWidth + 8
            height: 18
            radius: 4
            color: Qt.rgba(1, 1, 1, 0.08)

            Text {
              id: scoreText
              anchors.centerIn: parent
              text: root.score
              font.pixelSize: 10
              color: "#9a9996"
            }
          }
        }

        // Bottom Row: Snippet (if available)
        Text {
          id: snippetText
          width: parent.width
          text: root.snippet
          visible: root.snippet.length > 0
          font.pixelSize: 11
          color: "#c0bfbc"
          elide: Text.ElideRight
          maximumLineCount: 1
        }
      }
    }
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    onClicked: {
      ListView.view.currentIndex = root.index;
    }
    onDoubleClicked: {
      ListView.view.currentIndex = root.index;
      searchController.openFile(root.filePath);
    }
  }
}
