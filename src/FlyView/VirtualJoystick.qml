import QtQuick

import QGroundControl
import QGroundControl.Controls

Item {
    // The following properties must be passed in from the Loader
    // property bool autoCenterThrottle - true: throttle will snap back to center when released
    // property bool leftHandedMode - true: virtual joystick layout will be reversed

    id: virtualJoysticks

    readonly property bool stickActive:       leftStick.touchActive || rightStick.touchActive

    property var   _activeVehicle:            QGroundControl.multiVehicleManager.activeVehicle
    property bool  _initialConnectComplete:   _activeVehicle ? _activeVehicle.initialConnectComplete : false
    property real  leftYAxisValue:            autoCenterThrottle ? height / 2 : height
    property var   calibration:               false
    property real  uiTotalWidth:           0
    property real  uiRealX:                 0

    readonly property bool _boat: _activeVehicle ? _activeVehicle.boat : false
    readonly property bool _joystickEnabled: QGroundControl.settingsManager.appSettings.virtualJoystick.value
    readonly property bool _boatControlAvailable: _boat && _initialConnectComplete && visible && enabled
                                                 && _joystickEnabled && _activeVehicle.vehicleLinkManager
                                                 && !_activeVehicle.vehicleLinkManager.communicationLost
    readonly property bool _leftHanded: leftHandedMode
    readonly property bool _centerThrottle: autoCenterThrottle
    property bool _componentReady: false
    property bool _boatWasTarget: false
    property bool _boatInputReady: false

    Component.onCompleted: {
        _componentReady = true
        updateInputTarget()
    }
    on_ActiveVehicleChanged: updateInputTarget()
    on_BoatChanged: updateInputTarget()
    on_BoatControlAvailableChanged: {
        if (_boat) {
            cancelBoatInput()
        }
    }
    on_LeftHandedChanged: {
        if (_boat) {
            cancelBoatInput()
        }
    }
    on_CenterThrottleChanged: {
        if (_boat) {
            cancelBoatInput()
        }
    }
    onStickActiveChanged: {
        if (stickActive && _boatControlAvailable) {
            _boatInputReady = true
        }
    }

    function updateInputTarget() {
        if (!_componentReady) {
            return
        }
        if (_boat || _boatWasTarget) {
            cancelBoatInput()
        }
        _boatWasTarget = _boat
    }

    function cancelBoatInput() {
        if (!_componentReady) {
            return
        }
        _boatInputReady = false
        leftStick.cancelInput()
        rightStick.cancelInput()
        leftYAxisValue = leftStick.yAxis
    }

    Timer {
        objectName: "virtualJoystick_poll"
        interval:   40  // 25Hz, same as real joystick rate
        running:    _joystickEnabled
        repeat:     true
        onTriggered: {
            if (_activeVehicle && _initialConnectComplete && (!_boat || (_boatControlAvailable && _boatInputReady))) {
                leftHandedMode ? _activeVehicle.virtualTabletJoystickValue(leftStick.xAxis, leftStick.yAxis, rightStick.xAxis, rightStick.yAxis) : _activeVehicle.virtualTabletJoystickValue(rightStick.xAxis, rightStick.yAxis, leftStick.xAxis, leftStick.yAxis)
            }
            leftYAxisValue = leftStick.yAxis // We keep Y axis value from the throttle stick for using it while there is a resize
        }
    }

    onHeightChanged:        { keepYAxisWhileChanged() }
    onWidthChanged:         { keepXAxisWhileChanged() }
    onCalibrationChanged:   { calibration ? calibrateJoysticks() : undefined }

    function calibrateJoysticks() {
        if( virtualJoysticks.visible ) {
        keepXAxisWhileChanged()
        leftYAxisValue = leftStick.yAxisReCentered() // Keep track of the correct leftYAxisValue while the width is adjusted at first start up
        }
    }

    function keepYAxisWhileChanged () {
        if( virtualJoysticks.visible ) {
            leftStick.resize( leftYAxisValue )
            rightStick.reCenter()
        }
    }

    function keepXAxisWhileChanged () {
        if( virtualJoysticks.visible ) {
            leftStick.reCenter()
            rightStick.reCenter()
        }
    }

    JoystickThumbPad {
        id:                     leftStick
        objectName:             "virtualJoystick_leftStick"
        anchors.leftMargin:     xPositionDelta
        anchors.bottomMargin:   -yPositionDelta
        anchors.left:           parent.left
        anchors.bottom:         parent.bottom
        width:                  parent.height
        height:                 parent.height
        yAxisPositiveRangeOnly: _activeVehicle && !_activeVehicle.rover && !leftHandedMode
        yAxisReCenter:          autoCenterThrottle
    }

    JoystickThumbPad {
        id:                     rightStick
        objectName:             "virtualJoystick_rightStick"
        anchors.rightMargin:    -xPositionDelta
        anchors.bottomMargin:   -yPositionDelta
        anchors.right:          parent.right
        anchors.bottom:         parent.bottom
        width:                  parent.height
        height:                 parent.height
        yAxisPositiveRangeOnly: _activeVehicle && !_activeVehicle.rover && leftHandedMode
        yAxisReCenter:          true
    }
}
