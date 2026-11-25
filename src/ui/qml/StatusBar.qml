import QtQuick
import QtQuick.Controls

Item {
  id: root
  height: 24

  // Normal Status View (Idle / Search Results)
  Item {
    anchors.fill: parent
    visible: !searchController.isIndexing

    // Left Result Count (Shown when search returns results, like DigUp)
    Text {
      anchors.left: parent.left
      anchors.verticalCenter: parent.verticalCenter
      visible: searchController.resultsModel.rowCount() > 0
      text: {
        var count = searchController.resultsModel.rowCount();
        return count === 1 ? "1 result" : count + " results";
      }
      font.pixelSize: 11
      color: "#a0a59b"
    }

    // Right Total Indexed Files
    Text {
      anchors.right: parent.right
      anchors.verticalCenter: parent.verticalCenter
      text: searchController.totalFiles > 0 ? (searchController.totalFiles + " files indexed") : ""
      font.pixelSize: 11
      color: "#7f857b"
      visible: searchController.totalFiles > 0
    }
  }

  // Active Indexing Status View
  Row {
    anchors.fill: parent
    spacing: 8
    visible: searchController.isIndexing

    BusyIndicator {
      id: spinner
      width: 14
      height: 14
      anchors.verticalCenter: parent.verticalCenter
      running: searchController.isIndexing
    }

    Text {
      anchors.verticalCenter: parent.verticalCenter
      text: searchController.indexingStatus
      font.pixelSize: 11
      color: "#c8ff3d"
      elide: Text.ElideRight
      width: parent.width - 24
    }
  }
}
