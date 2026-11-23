import QtQml
import QtPositioning

QtObject {
    id: endpoint
    property int kind: 11
    readonly property bool boat: kind === 11
    readonly property bool fixedWing: kind === 1
    property bool armed: true
    property bool flying: false
    property bool landing: false
    property bool inFwdFlight: false
    property bool initialConnectComplete: true
    property int checkListState: 0
    property string flightMode: "Manual"
    property string pauseFlightMode: "Hold"
    property string rtlFlightMode: "Return"
    property string smartRTLFlightMode: "Smart Return"
    property string landFlightMode: "Land"
    property string missionFlightMode: "Mission"
    property var homePosition: QtPositioning.coordinate(47, 8, 400)
    property bool haveFWSpeedLimits: false
    property bool haveMRSpeedLimits: false
    property int sensorsPresentBits: 0
    property var vehicleLinkManager: QtObject { property bool communicationLost: false }
    property var healthAndArmingCheckReport: QtObject {
        property bool supported: false
        property bool canArm: true
        property bool canTakeoff: true
        property bool canStartMission: true
    }
    property var rcRSSI: QtObject { property real rawValue: 100 }
    property var altitudeRelative: QtObject {
        property real rawValue: NaN
        property real value: rawValue
    }
    property var supports: QtObject {
        property bool guidedMode: true
        property bool pauseVehicle: true
        property bool guidedTakeoffWithAltitude: endpoint.kind === 2 || endpoint.kind === 20
        property bool guidedTakeoffWithoutAltitude: endpoint.kind === 1
        property bool smartRTL: false
        property bool roiMode: false
        property bool orbitMode: false
    }
    property int pauseCount: 0
    property int altitudeCount: 0
    property int gotoCount: 0
    property int missionCount: 0
    property int rtlCount: 0
    property real lastAltitudeChange: NaN
    property bool lastPause: false
    function pauseVehicle() { pauseCount++ }
    function guidedModeChangeAltitude(change, pause) {
        altitudeCount++
        lastAltitudeChange = change
        lastPause = pause
    }
    function guidedModeGotoLocation(coordinate, radius) { gotoCount++; return true }
    function startMission() { missionCount++ }
    function guidedModeRTL(smart) { rtlCount++ }
}
