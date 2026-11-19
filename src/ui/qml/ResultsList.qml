import QtQuick
import QtQuick.Controls

Item {
  id: root

  signal itemActivated(string path)
  signal itemRevealed(string path)
  signal itemCopied(string path)

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
    if (listView.currentIndex >= 0 && listView.currentIndex < listView.count) {
      const p = searchController.resultsModel.pathAt(listView.currentIndex);
      if (p.length > 0) {
        searchController.openFile(p);
      }
    }
  }

  function revealCurrent() {
    if (listView.currentIndex >= 0 && listView.currentIndex < listView.count) {
      const p = searchController.resultsModel.pathAt(listView.currentIndex);
      if (p.length > 0) {
        searchController.revealInFolder(p);
      }
    }
  }

  function copyCurrent() {
    if (listView.currentIndex >= 0 && listView.currentIndex < listView.count) {
      const p = searchController.resultsModel.pathAt(listView.currentIndex);
      if (p.length > 0) {
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
      policy: ScrollBar.AsNeeded
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
