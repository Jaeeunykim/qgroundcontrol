import QtQuick
import QGroundControl.Toolbar

Item {
    id: harness
    width: 800
    height: 100
    property bool _communicationLost: false
    property color _mainStatusBGColor: "transparent"
    property alias vehicle: vehicle
    property alias indicator: indicator

    QtObject {
        id: mainWindow
        function showIndicatorDrawer(component, control) {}
    }

    GuidedActionsVehicleStub {
        id: vehicle
        property bool airShip: false
        readonly property bool vtol: kind === 20
        readonly property bool multiRotor: kind === 2
        property bool vtolInFwdFlight: false
        property int messageCount: 0
        property bool messageTypeWarning: false
        property bool messageTypeError: false
        property bool readyToFlyAvailable: true
        property bool readyToFly: true
    }

    MainStatusIndicator {
        id: indicator
        _activeVehicle: vehicle
    }
}
