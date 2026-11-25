import QtQuick
import QtQuick.Controls

Item {
  id: root

  signal itemActivated(string path)
  signal itemRevealed(string path)
  signal itemCopied(string path)

  property bool menuOpening: false
  property int openMenuCount: 0
  readonly property bool menuOpen: openMenuCount > 0 || menuOpening

  function selectNext() {
    if (listView.count === 0) return;
    listView.currentIndex = Math.min(listView.count - 1, listView.currentIndex + 1);
    listView.positionViewAtIndex(listView.currentIndex, ListView.Contain);
  }

  function selectPrevious() {
    if (listView.count === 0) return;
    listView.currentIndex = Math.max(0, listView.currentIndex - 1);
    listView.positionViewAtIndex(listView.currentIndex, ListView.Contain);
  }

  function activateCurrent() {
    var idx = listView.currentIndex;
    if (idx < 0 && searchController.resultsModel.rowCount() > 0) {
      idx = 0;
      listView.currentIndex = 0;
    }
    if (idx >= 0 && idx < searchController.resultsModel.rowCount()) {
      const p = searchController.resultsModel.pathAt(idx);
      const page = searchController.resultsModel.pageNumberAt(idx);
      if (p && p.length > 0) {
        searchController.openFile(p, page);
      }
    }
  }

  function revealCurrent() {
    var idx = listView.currentIndex;
    if (idx < 0 && searchController.resultsModel.rowCount() > 0) {
      idx = 0;
      listView.currentIndex = 0;
    }
    if (idx >= 0 && idx < searchController.resultsModel.rowCount()) {
      const p = searchController.resultsModel.pathAt(idx);
      if (p && p.length > 0) {
        searchController.revealInFolder(p);
      }
    }
  }

  function copyCurrent() {
    var idx = listView.currentIndex;
    if (idx < 0 && searchController.resultsModel.rowCount() > 0) {
      idx = 0;
      listView.currentIndex = 0;
    }
    if (idx >= 0 && idx < searchController.resultsModel.rowCount()) {
      const p = searchController.resultsModel.pathAt(idx);
      if (p && p.length > 0) {
        searchController.copyPath(p);
      }
    }
  }

  ListView {
    id: listView
    anchors.fill: parent
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    model: searchController.resultsModel
    delegate: ResultDelegate {}

    ScrollBar.vertical: ScrollBar {
      id: resultsScroll
      policy: ScrollBar.AsNeeded
      width: 4
      contentItem: Rectangle {
        implicitWidth: 4
        radius: 2
        color: resultsScroll.pressed ? Qt.rgba(255, 255, 255, 0.4) : (resultsScroll.hovered ? Qt.rgba(255, 255, 255, 0.25) : Qt.rgba(255, 255, 255, 0.16))
        Behavior on color { ColorAnimation { duration: 100 } }
      }
      background: Item {}
    }

    Connections {
      target: searchController
      function onSearchCompleted() {
        if (listView.count > 0) {
          listView.currentIndex = 0;
        }
      }
    }
  }
}
