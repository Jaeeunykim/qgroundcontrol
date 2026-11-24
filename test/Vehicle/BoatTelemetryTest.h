#pragma once

#include "UnitTest.h"

class BoatTelemetryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void init() override;
    void _initialState_data();
    void _initialState();
    void _groundSpeed_data();
    void _groundSpeed();
    void _timeToHomeUnits_data();
    void _timeToHomeUnits();
    void _invalidEstimateClearsPreviousValue();
    void _estimateFollowsDistanceChanges();
    void _headingAndTypeTransitions();
    void _headingAndCogAreIndependent();
    void _gpsCourseValidity_data();
    void _gpsCourseValidity();
    void _gpsCourseWithoutVehicle();
};
