#pragma once

#include "BaseClasses/VehicleTestManualConnect.h"

class BoatNavigationTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _gotoLocation_data();
    void _gotoLocation();
    void _pauseWithoutAltitude_data();
    void _pauseWithoutAltitude();
    void _rejectAircraftActions_data();
    void _rejectAircraftActions();
    void _missionAndReturn_data();
    void _missionAndReturn();
    void _commandResults_data();
    void _commandResults();
    void _invalidGotoCoordinates();
    void _planSpeedVehicleRoundTrip_data();
    void _planSpeedVehicleRoundTrip();
    void _rejectAircraftMission_data();
    void _rejectAircraftMission();
    void _virtualJoystickMapping_data();
    void _virtualJoystickMapping();
    void _virtualJoystickRejectsInvalidInput();

private:
    void _connectVehicle(MAV_TYPE type, bool commandInt = false, MAV_AUTOPILOT firmware = MAV_AUTOPILOT_PX4);
    bool _hasResult(const QSignalSpy& spy, MAV_RESULT result) const;
    void _checkManualControlCount(int expected);
};
