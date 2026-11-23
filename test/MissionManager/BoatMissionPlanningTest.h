#pragma once

#include "BaseClasses/MissionTest.h"

class BoatMissionPlanningTest : public OfflineMissionTest
{
    Q_OBJECT

private slots:
    void init() override;
    void _speedRoundTrip_data();
    void _speedRoundTrip();
    void _unmergedSpeedCommands_data();
    void _unmergedSpeedCommands();
    void _altitudePreservation_data();
    void _altitudePreservation();
    void _importCalculatedAltitude_data();
    void _importCalculatedAltitude();
    void _commandFilteringDoesNotLeak();
    void _availabilityFollowsOfflineType();
};
