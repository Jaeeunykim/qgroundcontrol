#pragma once

#include "UnitTest.h"

class Fact;

class AppSettingsTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _preferredFirmwareClassEnumFiltered();
    void _offlineEditingFirmwareClassEnumFiltered();
    void _offlineVehicleType_data();
    void _offlineVehicleType();

private:
    void _verifyFirmwareClassEnumFiltered(Fact *fact);
};
