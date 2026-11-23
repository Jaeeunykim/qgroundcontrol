import QtQuick
import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

Item {
    id: root
    width: 800
    height: 1000
    required property var activeVehicle
    property alias checklist: checklist
    QtObject { id: globals; property var activeVehicle: root.activeVehicle }
    QGCPalette { id: qgcPal }
    PreFlightCheckList { id: checklist; width: parent.width }
}
