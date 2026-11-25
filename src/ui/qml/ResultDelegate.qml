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

  function resultsListRoot() {
    return (root.ListView && root.ListView.view && root.ListView.view.parent) ? root.ListView.view.parent : null;
  }

  Rectangle {
    id: bg
    anchors.fill: parent
    anchors.leftMargin: 4
    anchors.rightMargin: 4
    anchors.topMargin: 1
    anchors.bottomMargin: 1
    radius: 8
    // Lime-tinted selected highlight matching landing theme
    color: root.isSelected ? Qt.rgba(200/255, 255/255, 61/255, 0.11) : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.04) : "transparent")
    border.color: root.isSelected ? Qt.rgba(200/255, 255/255, 61/255, 0.30) : "transparent"
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
      color: root.isSelected ? Qt.rgba(200/255, 255/255, 61/255, 0.16) : Qt.rgba(255, 255, 255, 0.05)

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
          if (root.isSelected) return "#c8ff3d";
          if (root.iconName === "image") return "#34d399";
          if (root.iconName === "video") return "#a78bfa";
          if (root.iconName === "audio") return "#f472b6";
          if (root.iconName === "code") return "#60a5fa";
          if (root.iconName === "pdf") return "#f87171";
          if (root.category === "doc") return "#fbbf24";
          return "#a0a59b";
        }
      }
    }

    function openContextMenu(useButtonAnchor) {
      const rl = root.resultsListRoot();
      if (rl) rl.menuOpening = true;
      if (root.ListView && root.ListView.view) {
        root.ListView.view.currentIndex = root.index;
      }
      if (useButtonAnchor) {
        contextMenu.popup(moreBtn, -contextMenu.implicitWidth + moreBtn.width, moreBtn.height + 4);
      } else {
        contextMenu.popup();
      }
    }

    // Subtle 3-dots action menu button on hovered/selected row (inside bg so siblings can anchor to it)
    Rectangle {
      id: moreBtn
      z: 5
      visible: root.isSelected || mouseArea.containsMouse || moreMouse.containsMouse || contextMenu.visible
      anchors.right: parent.right
      anchors.rightMargin: 6
      anchors.verticalCenter: parent.verticalCenter
      width: 24
      height: 24
      radius: 6
      color: moreMouse.containsMouse ? Qt.rgba(200/255, 255/255, 61/255, 0.16) : Qt.rgba(255, 255, 255, 0.05)
      border.color: moreMouse.containsMouse ? Qt.rgba(200/255, 255/255, 61/255, 0.35) : "transparent"
      border.width: 1

      Column {
        anchors.centerIn: parent
        spacing: 2.5

        Repeater {
          model: 3
          Rectangle {
            width: 2.5
            height: 2.5
            radius: 1.25
            color: moreMouse.containsMouse ? "#c8ff3d" : "#a0a59b"
          }
        }
      }

      MouseArea {
        id: moreMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
          bg.openContextMenu(true);
        }
      }
    }

    // Optional Keyframe or Image Thumbnail on Right Edge
    Rectangle {
      id: thumbPreview
      visible: root.thumbnailPath !== undefined && root.thumbnailPath.length > 0
      anchors.right: moreBtn.visible ? moreBtn.left : parent.right
      anchors.rightMargin: moreBtn.visible ? 6 : 8
      anchors.verticalCenter: parent.verticalCenter
      width: 32
      height: 20
      radius: 4
      clip: true
      color: "#070807"
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
      anchors.right: thumbPreview.visible ? thumbPreview.left : (moreBtn.visible ? moreBtn.left : parent.right)
      anchors.rightMargin: (thumbPreview.visible || moreBtn.visible) ? 8 : 10
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
            color: root.isSelected ? "#ffffff" : "#f3f5ef"
            elide: Text.ElideRight
            maximumLineCount: 1
          }

          // Page or Timestamp Pill
          Rectangle {
            id: pagePill
            height: 16
            width: pageLabel.implicitWidth + 8
            radius: 4
            color: root.isSelected ? Qt.rgba(200/255, 255/255, 61/255, 0.16) : Qt.rgba(255, 255, 255, 0.06)
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
              color: root.isSelected ? "#c8ff3d" : "#a0a59b"
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
          color: root.isSelected ? "#a0a59b" : "#7f857b"
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
        color: root.isSelected ? "#c6cbbd" : "#a0a59b"
        elide: Text.ElideRight
        maximumLineCount: 1
      }
    }

    MouseArea {
      id: mouseArea
      z: 1
      anchors.left: parent.left
      anchors.top: parent.top
      anchors.bottom: parent.bottom
      anchors.right: moreBtn.visible ? moreBtn.left : parent.right
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      acceptedButtons: Qt.LeftButton | Qt.RightButton

      onClicked: (mouse) => {
        if (mouse.button === Qt.RightButton) {
          bg.openContextMenu(false);
          return;
        }
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

  // ---------------------------------------------------------------------------
  // Right-click / 3-dots context menu
  // ---------------------------------------------------------------------------
  Menu {
    id: contextMenu
    implicitWidth: 184
    padding: 4
    margins: 8
    onOpened: {
      const rl = root.resultsListRoot();
      if (rl) {
        rl.openMenuCount += 1;
        rl.menuOpening = false;
      }
    }
    onClosed: {
      const rl = root.resultsListRoot();
      if (rl) {
        rl.openMenuCount = Math.max(0, rl.openMenuCount - 1);
        rl.menuOpening = false;
      }
    }
    background: Rectangle {
      implicitWidth: 184
      color: "#0e110d"
      border.color: Qt.rgba(255, 255, 255, 0.14)
      border.width: 1
      radius: 8
    }

    MenuItem {
      id: openMenuItem
      text: "Open"
      implicitHeight: 30
      onTriggered: {
        searchController.openFile(root.filePath, root.pageNumber);
      }
      contentItem: Text {
        text: openMenuItem.text
        font.pixelSize: 13
        color: openMenuItem.highlighted ? "#c8ff3d" : "#f3f5ef"
        leftPadding: 10
        verticalAlignment: Text.AlignVCenter
      }
      background: Rectangle {
        implicitHeight: 30
        color: openMenuItem.highlighted ? Qt.rgba(200/255, 255/255, 61/255, 0.12) : "transparent"
        radius: 6
      }
    }

    MenuItem {
      id: revealMenuItem
      text: "Show in Folder"
      implicitHeight: 30
      onTriggered: {
        searchController.revealInFolder(root.filePath);
      }
      contentItem: Text {
        text: revealMenuItem.text
        font.pixelSize: 13
        color: revealMenuItem.highlighted ? "#c8ff3d" : "#f3f5ef"
        leftPadding: 10
        verticalAlignment: Text.AlignVCenter
      }
      background: Rectangle {
        implicitHeight: 30
        color: revealMenuItem.highlighted ? Qt.rgba(200/255, 255/255, 61/255, 0.12) : "transparent"
        radius: 6
      }
    }

    MenuItem {
      id: copyPathMenuItem
      text: "Copy Path"
      implicitHeight: 30
      onTriggered: {
        searchController.copyPath(root.filePath);
      }
      contentItem: Text {
        text: copyPathMenuItem.text
        font.pixelSize: 13
        color: copyPathMenuItem.highlighted ? "#c8ff3d" : "#f3f5ef"
        leftPadding: 10
        verticalAlignment: Text.AlignVCenter
      }
      background: Rectangle {
        implicitHeight: 30
        color: copyPathMenuItem.highlighted ? Qt.rgba(200/255, 255/255, 61/255, 0.12) : "transparent"
        radius: 6
      }
    }

    // OCR copy: shown only for image results with extracted text
    MenuItem {
      id: copyOcrMenuItem
      text: "Copy Image Text"
      visible: root.category === "image" && searchController.getImageText(root.filePath).length > 0
      implicitHeight: visible ? 30 : 0
      height: visible ? 30 : 0
      onTriggered: {
        searchController.copyImageText(root.filePath);
      }
      contentItem: Text {
        text: copyOcrMenuItem.text
        font.pixelSize: 13
        color: "#c8ff3d"
        leftPadding: 10
        verticalAlignment: Text.AlignVCenter
      }
      background: Rectangle {
        implicitHeight: copyOcrMenuItem.visible ? 30 : 0
        color: copyOcrMenuItem.highlighted ? Qt.rgba(200/255, 255/255, 61/255, 0.12) : "transparent"
        radius: 6
      }
    }
  }
}

