#pragma once

#include "UnitTest.h"

class BoatChecklistTest : public UnitTest
{
    Q_OBJECT

private slots:
    void initTestCase() override;
    void _vehicleSwitch_data();
    void _vehicleSwitch();
    void _sameVehicleTypeChange();
};
