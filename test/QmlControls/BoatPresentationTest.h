#pragma once

#include "BaseClasses/MissionTest.h"

class FactValueGrid;
class InstrumentValueData;

class BoatPresentationTest : public OfflineMissionTest
{
    Q_OBJECT

private slots:
    void init() override;
    void _defaults_data();
    void _defaults();
    void _vehicleIcons_data();
    void _vehicleIcons();
    void _legacyLayoutMigration();
    void _existingBoatLayoutWins();
    void _mixedVehicleCards();

private:
    void _completeGrid(FactValueGrid& grid, const QString& group, bool card, Vehicle* cardVehicle = nullptr);
    InstrumentValueData* _value(FactValueGrid& grid, int column = 0, int row = 0);
};
