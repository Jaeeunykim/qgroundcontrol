import QtQuick
import QtPositioning
import QGroundControl.FlyView

// Real action controller, confirmation and toolbar entry, with deterministic telemetry/command endpoints.
Item {
    id: harness
    width: 800
    height: 600

    property alias controller: controller
    property alias vehicleA: vehicleA
    property alias vehicleB: vehicleB
    property alias dialog: dialog
    property alias slider: slider
    property alias takeoffAction: takeoffAction
    property alias selection: selection
    property alias mission: mission

    QtObject {
        id: mainWindow
        signal armVehicleRequest()
        signal forceArmVehicleRequest()
        signal disarmVehicleRequest()
    }

    GuidedActionsVehicleStub { id: vehicleA; kind: 11 }
    GuidedActionsVehicleStub { id: vehicleB; kind: 2; flying: true }

    QtObject {
        id: selection
        property int count: 0
        function get(index) { return index === 0 ? vehicleA : vehicleB }
    }

    QtObject {
        id: mission
        property bool containsItems: true
        property int currentMissionIndex: 1
        property int resumeMissionIndex: 1
        property int resumeCount: 0
        function resumeMission(index) { resumeCount++ }
        property var visualItems: QtObject { property int count: 4 }
        signal resumeMissionUploadFail()
    }

    Item {
        id: slider
        visible: false
        property int setupCount: 0
        function setupSlider(type, minimum, maximum, value, label) { setupCount++ }
        function getOutputValue() { return 50 }
    }

    QtObject { id: messageDisplayStub; property real opacity: 1 }
    Timer { id: messageFadeTimer; interval: 4000 }
    PropertyAnimation {
        id: messageOpacityAnimation
        target: messageDisplayStub
        property: "opacity"
        to: 0
    }

    GuidedActionsController {
        id: controller
        _activeVehicle: vehicleA
        _selectedVehicles: selection
        missionController: mission
        confirmDialog: dialog
        guidedValueSlider: slider
        _flyViewSettings: QtObject {
            property var enableAutomaticMissionPopups: QtObject { property bool rawValue: false }
            property var guidedMinimumAltitude: QtObject { property real value: 0 }
            property var guidedMaximumAltitude: QtObject { property real value: 100 }
            property var forwardFlightGoToLocationLoiterRad: QtObject { property real rawValue: 50 }
        }
    }

    GuidedActionConfirm {
        id: dialog
        guidedController: controller
        guidedValueSlider: slider
        messageDisplay: messageDisplayStub
    }

    GuidedActionTakeoff {
        id: takeoffAction
        _guidedController: controller
    }
}
