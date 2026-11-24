import QtQuick
import QGroundControl.FlyView

Item {
    id: harness
    width: 800
    height: 240

    property bool autoCenterThrottle: false
    property bool leftHandedMode: false
    property var selectedVehicle: vehicleA
    property alias vehicleA: vehicleA
    property alias vehicleB: vehicleB
    property alias controls: controls

    component Endpoint: QtObject {
        property int kind: 11
        readonly property bool boat: kind === 11
        readonly property bool rover: kind === 11 || kind === 10
        property bool initialConnectComplete: true
        property QtObject vehicleLinkManager: QtObject { property bool communicationLost: false }
        property int commandCount: 0
        property var lastAxes: []
        function virtualTabletJoystickValue(roll, pitch, yaw, thrust) {
            lastAxes = [roll, pitch, yaw, thrust]
            commandCount++
        }
    }

    Endpoint { id: vehicleA }
    Endpoint { id: vehicleB }

    VirtualJoystick {
        id: controls
        anchors.fill: parent
        _activeVehicle: harness.selectedVehicle
        uiTotalWidth: harness.width
    }
}
