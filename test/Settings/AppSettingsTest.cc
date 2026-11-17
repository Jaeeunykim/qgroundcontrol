#include "AppSettingsTest.h"

#include <QtCore/QSettings>
#include <QtTest/QSignalSpy>

#include "AppSettings.h"
#include "FirmwarePluginManager.h"
#include "Fixtures/RAIIFixtures.h"
#include "QGCMAVLink.h"
#include "SettingsManager.h"
#include "Vehicle.h"

UT_REGISTER_TEST(AppSettingsTest, TestLabel::Unit)

void AppSettingsTest::_preferredFirmwareClassEnumFiltered()
{
    _verifyFirmwareClassEnumFiltered(SettingsManager::instance()->appSettings()->preferredFirmwareClass());
}

void AppSettingsTest::_offlineEditingFirmwareClassEnumFiltered()
{
    _verifyFirmwareClassEnumFiltered(SettingsManager::instance()->appSettings()->offlineEditingFirmwareClass());
}

void AppSettingsTest::_offlineVehicleType_data()
{
    QTest::addColumn<int>("vehicleType");
    QTest::newRow("boat") << int(MAV_TYPE_SURFACE_BOAT);
    QTest::newRow("legacy-rover") << int(MAV_TYPE_GROUND_ROVER);
    QTest::newRow("multirotor") << int(MAV_TYPE_QUADROTOR);
    QTest::newRow("fixed-wing") << int(MAV_TYPE_FIXED_WING);
    QTest::newRow("vtol") << int(MAV_TYPE_VTOL_TAILSITTER_QUADROTOR);
    QTest::newRow("sub") << int(MAV_TYPE_SUBMARINE);
    QTest::newRow("unknown") << int(MAV_TYPE_GENERIC);
}

void AppSettingsTest::_offlineVehicleType()
{
    QFETCH(int, vehicleType);
    TestFixtures::SettingsFixture settingsFixture;
    AppSettings* const appSettings = SettingsManager::instance()->appSettings();
    Fact* const offlineType = appSettings->offlineEditingVehicleClass();
    Fact* const preferredType = appSettings->preferredVehicleClass();
    QVERIFY(offlineType->enumValues().contains(QVariant(vehicleType)));
    QVERIFY(preferredType->enumValues().contains(QVariant(vehicleType)));

    // Start from another type so the notification must be emitted for every row.
    offlineType->setRawValue(vehicleType == MAV_TYPE_SURFACE_BOAT ? MAV_TYPE_GROUND_ROVER : MAV_TYPE_SURFACE_BOAT);
    Vehicle vehicle(Vehicle::MAV_AUTOPILOT_TRACK, Vehicle::MAV_TYPE_TRACK);
    QSignalSpy typeChanged(&vehicle, &Vehicle::vehicleTypeChanged);
    QVERIFY(typeChanged.isValid());
    offlineType->setRawValue(vehicleType);
    QCOMPARE(typeChanged.count(), 1);
    QCOMPARE(int(vehicle.vehicleType()), vehicleType);
    QCOMPARE(vehicle.boat(), vehicleType == MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(vehicle.property("boat").toBool(), vehicle.boat());
    QCOMPARE(vehicle.rover(), vehicleType == MAV_TYPE_SURFACE_BOAT || vehicleType == MAV_TYPE_GROUND_ROVER);

    QSettings settings;
    settings.sync();
    QCOMPARE(settings.value(offlineType->name()).toInt(), vehicleType);
    // Recreating the offline vehicle must retain the selected type.
    Vehicle restoredVehicle(Vehicle::MAV_AUTOPILOT_TRACK, Vehicle::MAV_TYPE_TRACK);
    QCOMPARE(int(restoredVehicle.vehicleType()), vehicleType);
    QCOMPARE(restoredVehicle.boat(), vehicle.boat());

    if (vehicleType != MAV_TYPE_GENERIC) {
        settingsFixture.setFactValue(preferredType, MAV_TYPE_GENERIC);
        preferredType->setRawValue(vehicleType);
        QCOMPARE(offlineType->rawValue().toInt(), vehicleType);
    }
}

void AppSettingsTest::_verifyFirmwareClassEnumFiltered(Fact *fact)
{
    const QList<QGCMAVLink::FirmwareClass_t> supportedClasses = FirmwarePluginManager::instance()->supportedFirmwareClasses();

    const QVariantList enumValues = fact->enumValues();
    QCOMPARE(fact->enumStrings().count(), enumValues.count());
    QVERIFY(!enumValues.isEmpty());

    for (const QVariant &enumValue : enumValues) {
        const auto firmwareClass = static_cast<QGCMAVLink::FirmwareClass_t>(enumValue.toUInt());
        QVERIFY2(supportedClasses.contains(firmwareClass),
                 qPrintable(QStringLiteral("%1 enum offers unsupported firmware class %2").arg(fact->name()).arg(enumValue.toUInt())));
    }

    QVERIFY2(supportedClasses.contains(static_cast<QGCMAVLink::FirmwareClass_t>(fact->rawValue().toUInt())),
             qPrintable(QStringLiteral("%1 value is an unsupported firmware class %2").arg(fact->name()).arg(fact->rawValue().toUInt())));
}
