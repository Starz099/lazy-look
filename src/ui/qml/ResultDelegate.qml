import QtQuick
import QtQuick.Controls

Item {
  id: root
  width: ListView.view ? ListView.view.width : 0
  height: snippetText.text.length > 0 ? 52 : 40

  required property int index
  required property string filePath
  required property string fileName
  required property string directory
  required property string score
  required property string snippet
  required property string category
  required property string iconName
  required property int pageNumber
  required property string thumbnailPath

  property bool isSelected: ListView.isCurrentItem

  Rectangle {
    id: bg
    anchors.fill: parent
    anchors.leftMargin: 4
    anchors.rightMargin: 4
    anchors.topMargin: 1
    anchors.bottomMargin: 1
    radius: 8
    // DigUp slate-blue selected highlight
    color: root.isSelected ? "#3f5675" : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.04) : "transparent")
    border.color: root.isSelected ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
    border.width: 1

    Behavior on color {
      ColorAnimation { duration: 60 }
    }

    // Category / File Type Vector Icon Squircle
    Rectangle {
      id: iconSquircle
      width: 28
      height: 28
      radius: 7
      anchors.left: parent.left
      anchors.leftMargin: 8
      anchors.verticalCenter: parent.verticalCenter
      color: root.isSelected ? Qt.rgba(255, 255, 255, 0.16) : Qt.rgba(255, 255, 255, 0.06)

      Icon {
        anchors.centerIn: parent
        size: 14
        name: {
          if (root.iconName === "image") return "image";
          if (root.iconName === "video") return "video";
          if (root.iconName === "audio") return "audio";
          if (root.iconName === "code") return "code";
          if (root.iconName === "pdf" || root.iconName === "markdown" || root.iconName === "doc" || root.category === "doc") return "doc";
          return "file";
        }
        color: {
          if (root.isSelected) return "#ffffff";
          if (root.iconName === "image") return "#34d399";
          if (root.iconName === "video") return "#a78bfa";
          if (root.iconName === "audio") return "#f472b6";
          if (root.iconName === "code") return "#60a5fa";
          if (root.iconName === "pdf") return "#f87171";
          if (root.category === "doc") return "#fbbf24";
          return "#94a3b8";
        }
      }
    }

    // Optional Keyframe or Image Thumbnail on Right Edge
    Rectangle {
      id: thumbPreview
      visible: root.thumbnailPath !== undefined && root.thumbnailPath.length > 0
      anchors.right: parent.right
      anchors.rightMargin: 8
      anchors.verticalCenter: parent.verticalCenter
      width: 32
      height: 20
      radius: 4
      clip: true
      color: "#1e1d1c"
      border.color: Qt.rgba(255, 255, 255, 0.1)
      border.width: 1

      Image {
        anchors.fill: parent
        source: (root.thumbnailPath !== undefined && root.thumbnailPath.length > 0) ? ("file://" + root.thumbnailPath) : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
      }
    }

    // Main Content Details Area
    Column {
      id: textCol
      anchors.left: iconSquircle.right
      anchors.leftMargin: 10
      anchors.right: thumbPreview.visible ? thumbPreview.left : parent.right
      anchors.rightMargin: thumbPreview.visible ? 8 : 10
      anchors.verticalCenter: parent.verticalCenter
      spacing: 2

      // Top Row: File Name + Optional Page Pill + Directory Path
      Item {
        id: topRow
        width: parent.width
        height: 18

        Row {
          id: labelRow
          anchors.left: parent.left
          anchors.verticalCenter: parent.verticalCenter
          spacing: 6

          Text {
            id: fileNameText
            text: root.fileName
            font.pixelSize: 13
            font.weight: Font.Medium
            color: root.isSelected ? "#ffffff" : "#f5f5f5"
            elide: Text.ElideRight
            maximumLineCount: 1
          }

          // Page or Timestamp Pill
          Rectangle {
            id: pagePill
            height: 16
            width: pageLabel.implicitWidth + 8
            radius: 4
            color: root.isSelected ? "#2e435e" : "#3a3837"
            visible: root.pageNumber > 0
            anchors.verticalCenter: parent.verticalCenter

            Text {
              id: pageLabel
              anchors.centerIn: parent
              text: {
                if (root.category === "video" || root.category === "audio" || root.iconName === "video" || root.iconName === "audio") {
                  var m = Math.floor(root.pageNumber / 60);
                  var s = root.pageNumber % 60;
                  return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
                }
                return "p. " + root.pageNumber;
              }
              font.pixelSize: 10
              color: root.isSelected ? "#e0e8f5" : "#cccccc"
            }
          }
        }

        Text {
          id: dirText
          anchors.left: labelRow.right
          anchors.leftMargin: 8
          anchors.right: parent.right
          anchors.verticalCenter: parent.verticalCenter
          text: root.directory
          font.pixelSize: 11
          color: root.isSelected ? "#c8d4e4" : "#9e9c99"
          elide: Text.ElideMiddle
          maximumLineCount: 1
        }
      }

      // Bottom Row: Context Match Snippet
      Text {
        id: snippetText
        width: parent.width
        text: root.snippet
        visible: root.snippet.length > 0
        font.pixelSize: 11
        color: root.isSelected ? "#d4e0ee" : "#a8a6a3"
        elide: Text.ElideRight
        maximumLineCount: 1
      }
    }
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    onClicked: {
      if (root.ListView && root.ListView.view) {
        root.ListView.view.currentIndex = root.index;
      }
      var targetPath = (root.filePath && root.filePath.length > 0) ? root.filePath : searchController.resultsModel.pathAt(root.index);
      var targetPage = (root.pageNumber !== undefined && root.pageNumber > 0) ? root.pageNumber : searchController.resultsModel.pageNumberAt(root.index);
      if (targetPath && targetPath.length > 0) {
        searchController.openFile(targetPath, targetPage);
      }
    }
  }
}
