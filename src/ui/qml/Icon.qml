import QtQuick
import QtQuick.Shapes

Item {
  id: root
  width: size
  height: size

  property int size: 16
  property color color: "#a1a1aa"
  property string name: "search"
  property real strokeWidth: 1.75
  property bool filled: false

  // Lucide 24x24 standardized vector path definitions
  readonly property var pathMap: ({
    "search": "M 21 21 L 16.65 16.65 M 19 11 A 8 8 0 1 1 3 11 A 8 8 0 0 1 19 11 Z",
    "settings": "M 4 21 V 14 M 4 10 V 3 M 12 21 V 12 M 12 8 V 3 M 20 21 V 16 M 20 12 V 3 M 1 14 H 7 M 9 8 H 15 M 17 16 H 23",
    "close": "M 18 6 L 6 18 M 6 6 L 18 18",
    "folder": "M 20 20 A 2 2 0 0 0 22 18 V 8 A 2 2 0 0 0 20 6 H 12.1 A 2 2 0 0 1 10.41 5.1 L 9.6 3.9 A 2 2 0 0 0 7.93 3 H 4 A 2 2 0 0 0 2 5 V 18 A 2 2 0 0 0 4 20 Z",
    "trash": "M 3 6 H 21 M 19 6 V 20 A 2 2 0 0 1 17 22 H 7 A 2 2 0 0 1 5 20 V 6 M 8 6 V 4 A 2 2 0 0 1 10 2 H 14 A 2 2 0 0 1 16 4 V 6 M 10 11 V 17 M 14 11 V 17",
    "code": "M 16 18 L 22 12 L 16 6 M 8 6 L 2 12 L 8 18",
    "doc": "M 14.5 2 H 6 A 2 2 0 0 0 4 4 V 20 A 2 2 0 0 0 6 22 H 18 A 2 2 0 0 0 20 20 V 7.5 L 14.5 2 Z M 14 2 V 8 H 20 M 16 13 H 8 M 16 17 H 8 M 10 9 H 8",
    "document": "M 14.5 2 H 6 A 2 2 0 0 0 4 4 V 20 A 2 2 0 0 0 6 22 H 18 A 2 2 0 0 0 20 20 V 7.5 L 14.5 2 Z M 14 2 V 8 H 20 M 16 13 H 8 M 16 17 H 8 M 10 9 H 8",
    "image": "M 19 3 H 5 A 2 2 0 0 0 3 5 V 19 A 2 2 0 0 0 5 21 H 19 A 2 2 0 0 0 21 19 V 5 A 2 2 0 0 0 19 3 Z M 8.5 10 A 1.5 1.5 0 1 0 8.5 7 A 1.5 1.5 0 0 0 8.5 10 Z M 21 15 L 16 10 L 5 21",
    "video": "M 15 10 L 21 6.5 V 17.5 L 15 14 M 3 6 A 2 2 0 0 1 5 4 H 13 A 2 2 0 0 1 15 6 V 18 A 2 2 0 0 1 13 20 H 5 A 2 2 0 0 1 3 18 Z",
    "audio": "M 11 5 L 6 9 H 2 V 15 H 6 L 11 19 V 5 Z M 15.54 8.46 A 5 5 0 0 1 15.54 15.54 M 19.07 4.93 A 10 10 0 0 1 19.07 19.07",
    "sync": "M 21.5 2 V 8 H 15.5 M 21.34 15.57 A 10 10 0 1 1 20.77 7.19 L 21.5 8",
    "stop": "M 9 9 H 15 V 15 H 9 Z M 21 12 A 9 9 0 1 1 3 12 A 9 9 0 0 1 21 12 Z",
    "download": "M 21 15 V 19 A 2 2 0 0 1 19 21 H 5 A 2 2 0 0 1 3 19 V 15 M 7 10 L 12 15 L 17 10 M 12 15 V 3",
    "check": "M 20 6 L 9 17 L 4 12",
    "sparkle": "M 12 3 L 13.91 8.89 A 2 2 0 0 0 15.11 10.09 L 21 12 L 15.11 13.91 A 2 2 0 0 0 13.91 15.11 L 12 21 L 10.09 15.11 A 2 2 0 0 0 8.89 13.91 L 3 12 L 8.89 10.09 A 2 2 0 0 0 10.09 8.89 Z",
    "file": "M 14.5 2 H 6 A 2 2 0 0 0 4 4 V 20 A 2 2 0 0 0 6 22 H 18 A 2 2 0 0 0 20 20 V 7.5 L 14.5 2 Z M 14 2 V 8 H 20",
    "chevron-right": "M 9 18 L 15 12 L 9 6",
    "clock": "M 12 6 V 12 L 16 14 M 21 12 A 9 9 0 1 1 3 12 A 9 9 0 0 1 21 12 Z",
    "arrow-up-right": "M 7 17 L 17 7 M 7 7 H 17 V 17"
  })

  readonly property string currentSvgPath: pathMap[root.name] || pathMap["file"]

  Item {
    id: canvasContainer
    width: 24
    height: 24
    scale: root.size / 24.0
    transformOrigin: Item.TopLeft

    Shape {
      anchors.fill: parent
      preferredRendererType: Shape.CurveRenderer

      ShapePath {
        strokeColor: root.filled ? "transparent" : root.color
        strokeWidth: root.strokeWidth
        fillColor: root.filled ? root.color : "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin

        PathSvg {
          path: root.currentSvgPath
        }
      }
    }
  }
}
