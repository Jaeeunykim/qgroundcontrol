#pragma once

#include "BaseClasses/VehicleTestManualConnect.h"

class Fact;

class BoatLinkRecoveryTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _lossAndRecovery_data();
    void _lossAndRecovery();

private:
    void _connectVehicle(MAV_AUTOPILOT autopilot, MAV_TYPE type);
    bool _receive(const mavlink_message_t& message);
    QList<Fact*> _instruments() const;
};
