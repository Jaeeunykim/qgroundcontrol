#include "SpeedSection.h"
#include "SimpleMissionItem.h"
#include "PlanMasterController.h"
#include "Vehicle.h"
#include "QmlObjectListModel.h"

const char* SpeedSection::_flightSpeedName = "FlightSpeed";

QMap<QString, FactMetaData*> SpeedSection::_metaDataMap;

SpeedSection::SpeedSection(PlanMasterController* masterController, QObject* parent)
    : Section               (masterController, parent)
    , _available            (false)
    , _dirty                (false)
    , _specifyFlightSpeed   (false)
    , _flightSpeedFact      (0, _flightSpeedName,   FactMetaData::valueTypeDouble)
{
    if (_metaDataMap.isEmpty()) {
        _metaDataMap = FactMetaData::createMapFromJsonFile(QStringLiteral(":/json/SpeedSection.FactMetaData.json"), nullptr /* metaDataParent */);
    }

    double flightSpeed = 0;
    if (_masterController->controllerVehicle()->multiRotor()) {
        flightSpeed = _masterController->controllerVehicle()->defaultHoverSpeed();
    } else {
        flightSpeed = _masterController->controllerVehicle()->defaultCruiseSpeed();
    }

    _metaDataMap[_flightSpeedName]->setRawDefaultValue(flightSpeed);
    _flightSpeedFact.setMetaData(_metaDataMap[_flightSpeedName]);
    _flightSpeedFact.setRawValue(flightSpeed);

    connect(this,               &SpeedSection::specifyFlightSpeedChanged,   this, &SpeedSection::settingsSpecifiedChanged);
    connect(&_flightSpeedFact,  &Fact::valueChanged,                        this, &SpeedSection::_flightSpeedChanged);

    connect(this,               &SpeedSection::specifyFlightSpeedChanged,   this, &SpeedSection::_updateSpecifiedFlightSpeed);
    connect(&_flightSpeedFact,  &Fact::valueChanged,                        this, &SpeedSection::_updateSpecifiedFlightSpeed);

    const Vehicle* vehicle = _masterController->controllerVehicle();
    if (vehicle) {
        connect(vehicle, &Vehicle::vehicleTypeChanged, this, [this]() { setAvailable(_requestedAvailable); });
    }
}

bool SpeedSection::settingsSpecified(void) const
{
    return _specifyFlightSpeed;
}

void SpeedSection::setAvailable(bool available)
{
    _requestedAvailable = available;
    const Vehicle* vehicle = _masterController->controllerVehicle();
    const bool effectiveAvailable =
        available && vehicle && (vehicle->boat() || vehicle->multiRotor() || vehicle->fixedWing());
    if (effectiveAvailable != _available) {
        _available = effectiveAvailable;
        emit availableChanged(_available);
    }
}

void SpeedSection::setDirty(bool dirty)
{
    if (_dirty != dirty) {
        _dirty = dirty;
        emit dirtyChanged(_dirty);
    }
}

void SpeedSection::setSpecifyFlightSpeed(bool specifyFlightSpeed)
{
    if (specifyFlightSpeed != _specifyFlightSpeed) {
        _specifyFlightSpeed = specifyFlightSpeed;
        emit specifyFlightSpeedChanged(specifyFlightSpeed);
        setDirty(true);
        emit itemCountChanged(itemCount());
    }
}

int SpeedSection::itemCount(void) const
{
    return _specifyFlightSpeed ? 1: 0;
}

void SpeedSection::appendSectionItems(QList<MissionItem*>& items, QObject* missionItemParent, int& seqNum)
{
    // IMPORTANT NOTE: If anything changes here you must also change SpeedSection::scanForSettings

    const Vehicle* vehicle = _masterController->controllerVehicle();
    if (_specifyFlightSpeed && vehicle) {
        MissionItem* item = new MissionItem(seqNum++,
                                            MAV_CMD_DO_CHANGE_SPEED,
                                            MAV_FRAME_MISSION,
                                            (vehicle->multiRotor() || vehicle->boat()) ? 1 /* groundspeed */ : 0 /* airspeed */,
                                            _flightSpeedFact.rawValue().toDouble(),
                                            -1,                                                                 // No throttle change
                                            0,                                                                  // Absolute speed change
                                            0, 0, 0,                                                            // param 5-7 not used
                                            true,                                                               // autoContinue
                                            false,                                                              // isCurrentItem
                                            missionItemParent);
        items.append(item);
    }
}

bool SpeedSection::scanForSection(QmlObjectListModel* visualItems, int scanIndex)
{
    const Vehicle* vehicle = _masterController->controllerVehicle();
    if (!vehicle || !_available || scanIndex < 0 || scanIndex >= visualItems->count()) {
        return false;
    }

    SimpleMissionItem* item = visualItems->value<SimpleMissionItem*>(scanIndex);
    if (!item) {
        // We hit a complex item, there can't be a speed setting
        return false;
    }
    MissionItem& missionItem = item->missionItem();

    // See SpeedSection::appendMissionItems for specs on what consitutes a known speed setting

    if (missionItem.command() == MAV_CMD_DO_CHANGE_SPEED && missionItem.param3() == -1 && missionItem.param4() == 0 && missionItem.param5() == 0 && missionItem.param6() == 0 && missionItem.param7() == 0) {
        // Preserve sentinel/invalid commands as raw items instead of consuming them into a nonnegative-speed Fact.
        if (vehicle->boat() && (!qIsFinite(missionItem.param2()) || missionItem.param2() < 0)) {
            return false;
        }
        if ((vehicle->multiRotor() || vehicle->boat()) && missionItem.param1() != 1) {
            return false;
        } else if (vehicle->fixedWing() && missionItem.param1() != 0) {
            return false;
        }
        visualItems->removeAt(scanIndex)->deleteLater();
        _flightSpeedFact.setRawValue(missionItem.param2());
        setSpecifyFlightSpeed(true);
        return true;
    }

    return false;
}


double SpeedSection::specifiedFlightSpeed(void) const
{
    return _specifyFlightSpeed ? _flightSpeedFact.rawValue().toDouble() : std::numeric_limits<double>::quiet_NaN();
}

void SpeedSection::_updateSpecifiedFlightSpeed(void)
{
    if (_specifyFlightSpeed) {
        emit specifiedFlightSpeedChanged(specifiedFlightSpeed());
    }
}

void SpeedSection::_flightSpeedChanged(void)
{
    // We only set the dirty bit if specify flight speed it set. This allows us to change defaults for flight speed
    // without affecting dirty.
    if (_specifyFlightSpeed) {
        setDirty(true);
    }
}
