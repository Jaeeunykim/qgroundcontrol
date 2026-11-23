import QtQuick
import QtQuick.Controls
import QtQml.Models

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView

Item {
    id: root
    property var model: listModel
    readonly property var _vehicle: globals.activeVehicle ? globals.activeVehicle : QGroundControl.multiVehicleManager.offlineEditingVehicle
    readonly property bool _isBoat: _vehicle ? _vehicle.boat : false
    PreFlightCheckModel {
        id:     listModel
        PreFlightCheckGroup {
            objectName: "roverChecklist_initialChecks"
            name: root._isBoat ? qsTr("Boat Initial Checks") : qsTr("Rover Initial Checks")

            PreFlightCheckButton {
                name:           qsTr("Hardware")
                manualText:     qsTr("Battery mounted and secured?")
            }

            PreFlightBatteryCheck {
                failurePercent:                 40
                allowFailurePercentOverride:    false
            }

            PreFlightSensorsHealthCheck {
            }

            PreFlightGPSCheck {
                failureSatCount:        9
                allowOverrideSatCount:  true
            }

            PreFlightRCCheck {
            }
        }

        PreFlightCheckGroup {
            name: qsTr("Please arm the vehicle here")

            PreFlightCheckButton {
                objectName:     "roverChecklist_missionCheck"
                name:           qsTr("Mission")
                manualText:     root._isBoat ? qsTr("Please confirm waypoints and the water route are valid for this vessel.") : qsTr("Please confirm mission is valid (waypoints valid, no terrain collision).")
            }

            PreFlightSoundCheck {
            }
        }

        PreFlightCheckGroup {
            name: root._isBoat ? qsTr("Last preparations before departure") : qsTr("Last preparations before launch")

            // Check list item group 2 - Final checks before launch
            PreFlightCheckButton {
                name:           qsTr("Payload")
                manualText:     qsTr("Configured and started? Payload lid closed?")
            }

            PreFlightCheckButton {
                name:           qsTr("Wind & weather")
                manualText:     qsTr("OK for your platform?")
            }

            PreFlightCheckButton {
                name:           qsTr("Mission area")
                manualText:     qsTr("Mission area and path free of obstacles/people?")
            }
        }
    }
}
