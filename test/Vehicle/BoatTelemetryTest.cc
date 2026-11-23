#include "BoatTelemetryTest.h"

#include <QtCore/QRegularExpression>
#include <limits>

#include "AppSettings.h"
#include "MultiVehicleManager.h"
#include "SettingsManager.h"
#include "UnitsSettings.h"
#include "Vehicle.h"

UT_REGISTER_TEST(BoatTelemetryTest, TestLabel::Unit, TestLabel::Vehicle)

void BoatTelemetryTest::init()
{
    UnitTest::init();
    // Changing display units deliberately produces the setting's restart notice.
    ignoreLogMessage("API.QGCApplication.AppMessage", QtDebugMsg,
                     QRegularExpression(QStringLiteral("Restart application for changes to take effect")));
}

namespace {
void sendSpeed(Vehicle& vehicle, float speed)
{
    mavlink_vfr_hud_t hud{};
    hud.groundspeed = speed;
    mavlink_message_t message{};
    mavlink_msg_vfr_hud_encode(vehicle.id(), vehicle.compId(), &message, &hud);
    vehicle.VehicleFactGroup::handleMessage(&vehicle, message);
}
}  // namespace

void BoatTelemetryTest::_initialState_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("boat") << int(MAV_TYPE_SURFACE_BOAT);
    QTest::newRow("rover") << int(MAV_TYPE_GROUND_ROVER);
    QTest::newRow("multirotor") << int(MAV_TYPE_QUADROTOR);
}

void BoatTelemetryTest::_initialState()
{
    QFETCH(int, type);
    Vehicle vehicle(MAV_AUTOPILOT_PX4, static_cast<MAV_TYPE>(type));
    for (Fact* fact : {vehicle.heading(), vehicle.groundSpeed(), vehicle.distanceToHome(), vehicle.distanceToNextWP(),
                       vehicle.timeToHome()}) {
        if (type == MAV_TYPE_SURFACE_BOAT) {
            QVERIFY(qIsNaN(fact->rawValue().toDouble()));
        }
    }
    if (type != MAV_TYPE_SURFACE_BOAT) {
        QCOMPARE(vehicle.heading()->rawValue().toDouble(), 0.0);
        QCOMPARE(vehicle.groundSpeed()->rawValue().toDouble(), 0.0);
    }
}

void BoatTelemetryTest::_groundSpeed_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<float>("speed");
    for (const int type : {int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_QUADROTOR)}) {
        for (const auto& sample :
             QList<QPair<QString, float>>{{QStringLiteral("zero"), 0},
                                          {QStringLiteral("moving"), 2.5f},
                                          {QStringLiteral("nan"), qQNaN()},
                                          {QStringLiteral("infinity"), std::numeric_limits<float>::infinity()},
                                          {QStringLiteral("negative"), -1}}) {
            QTest::newRow(QStringLiteral("type%1-%2").arg(type).arg(sample.first).toLatin1().constData())
                << type << sample.second;
        }
    }
}

void BoatTelemetryTest::_groundSpeed()
{
    QFETCH(int, type);
    QFETCH(float, speed);
    SettingsManager::instance()->unitsSettings()->horizontalDistanceUnits()->setRawValue(
        UnitsSettings::HorizontalDistanceUnitsMeters);
    Vehicle vehicle(MAV_AUTOPILOT_PX4, static_cast<MAV_TYPE>(type));
    vehicle.distanceToHome()->setRawValue(100.0);
    sendSpeed(vehicle, speed);
    const bool invalidBoat = type == MAV_TYPE_SURFACE_BOAT && (!qIsFinite(speed) || speed < 0);
    if (invalidBoat) {
        QVERIFY(qIsNaN(vehicle.groundSpeed()->rawValue().toDouble()));
        QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
    } else {
        const double expected = type != MAV_TYPE_SURFACE_BOAT && qIsNaN(speed) ? 0 : double(speed);
        QCOMPARE(vehicle.groundSpeed()->rawValue().toDouble(), expected);
        if (type == MAV_TYPE_SURFACE_BOAT) {
            if (speed == 0) {
                QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
            } else {
                QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 100.0 / speed);
            }
        }
    }
}

void BoatTelemetryTest::_timeToHomeUnits_data()
{
    QTest::addColumn<int>("distanceUnits");
    QTest::addColumn<int>("speedUnits");
    for (const int distance :
         {int(UnitsSettings::HorizontalDistanceUnitsMeters), int(UnitsSettings::HorizontalDistanceUnitsFeet)}) {
        for (const int speed : {int(UnitsSettings::SpeedUnitsMetersPerSecond), int(UnitsSettings::SpeedUnitsKnots)}) {
            QTest::newRow(QStringLiteral("distance%1-speed%2").arg(distance).arg(speed).toLatin1().constData())
                << distance << speed;
        }
    }
}

void BoatTelemetryTest::_timeToHomeUnits()
{
    QFETCH(int, distanceUnits);
    QFETCH(int, speedUnits);
    SettingsManager::instance()->unitsSettings()->horizontalDistanceUnits()->setRawValue(distanceUnits);
    SettingsManager::instance()->unitsSettings()->speedUnits()->setRawValue(speedUnits);
    Vehicle vehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_SURFACE_BOAT);
    vehicle.distanceToHome()->setRawValue(100.0);
    sendSpeed(vehicle, 2.0f);
    QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 50.0);
    if (distanceUnits == UnitsSettings::HorizontalDistanceUnitsFeet) {
        QVERIFY(vehicle.distanceToHome()->cookedValue().toDouble() > 300.0);
    }
    if (speedUnits == UnitsSettings::SpeedUnitsKnots) {
        QVERIFY(vehicle.groundSpeed()->cookedValue().toDouble() > 3.0);
    }
}

void BoatTelemetryTest::_invalidEstimateClearsPreviousValue()
{
    Vehicle vehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_SURFACE_BOAT);
    for (const double distance : {qQNaN(), -1.0, std::numeric_limits<double>::infinity()}) {
        vehicle.distanceToHome()->setRawValue(100.0);
        sendSpeed(vehicle, 2.0f);
        QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 50.0);
        vehicle.distanceToHome()->setRawValue(distance);
        sendSpeed(vehicle, 2.0f);
        QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
    }
    vehicle.distanceToHome()->setRawValue(100.0);
    sendSpeed(vehicle, 2.0f);
    sendSpeed(vehicle, qQNaN());
    QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
    sendSpeed(vehicle, 0.0f);
    QCOMPARE(vehicle.groundSpeed()->rawValue().toDouble(), 0.0);
    QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
    sendSpeed(vehicle, 4.0f);
    QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 25.0);
}

void BoatTelemetryTest::_headingAndTypeTransitions()
{
    auto* type = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    MultiVehicleManager::instance()->init();
    auto* vehicle = MultiVehicleManager::instance()->offlineEditingVehicle();
    QVERIFY(vehicle);
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    QVERIFY(qIsNaN(vehicle->heading()->rawValue().toDouble()));
    mavlink_attitude_t attitude{};
    mavlink_message_t message{};
    mavlink_msg_attitude_encode(vehicle->id(), vehicle->compId(), &message, &attitude);
    vehicle->VehicleFactGroup::handleMessage(vehicle, message);
    QCOMPARE(vehicle->heading()->rawValue().toDouble(), 0.0);
    attitude.yaw = qQNaN();
    mavlink_msg_attitude_encode(vehicle->id(), vehicle->compId(), &message, &attitude);
    vehicle->VehicleFactGroup::handleMessage(vehicle, message);
    QVERIFY(qIsNaN(vehicle->heading()->rawValue().toDouble()));
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    vehicle->heading()->setRawValue(90.0);
    vehicle->groundSpeed()->setRawValue(5.0);
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    QVERIFY(qIsNaN(vehicle->heading()->rawValue().toDouble()));
    QVERIFY(qIsNaN(vehicle->groundSpeed()->rawValue().toDouble()));
}

void BoatTelemetryTest::_headingAndCogAreIndependent()
{
    Vehicle vehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_SURFACE_BOAT);
    mavlink_attitude_t attitude{};
    attitude.yaw = qDegreesToRadians(90.0f);
    mavlink_message_t message{};
    mavlink_msg_attitude_encode(vehicle.id(), vehicle.compId(), &message, &attitude);
    vehicle.VehicleFactGroup::handleMessage(&vehicle, message);
    QCOMPARE(vehicle.heading()->rawValue().toDouble(), 90.0);
    auto* gps = vehicle.gpsFactGroup();
    QVERIFY(gps);
    auto* cog = gps->getFact(QStringLiteral("courseOverGround"));
    QVERIFY(cog);
    mavlink_gps_raw_int_t position{};
    position.fix_type = 3;
    position.satellites_visible = 12;
    position.cog = 27000;
    mavlink_msg_gps_raw_int_encode(vehicle.id(), vehicle.compId(), &message, &position);
    gps->handleMessage(&vehicle, message);
    QCOMPARE(cog->rawValue().toDouble(), 270.0);
    QCOMPARE(vehicle.heading()->rawValue().toDouble(), 90.0);
    position.cog = UINT16_MAX;
    mavlink_msg_gps_raw_int_encode(vehicle.id(), vehicle.compId(), &message, &position);
    gps->handleMessage(&vehicle, message);
    QVERIFY(qIsNaN(cog->rawValue().toDouble()));
    QCOMPARE(vehicle.heading()->rawValue().toDouble(), 90.0);
    position.cog = 0;
    mavlink_msg_gps_raw_int_encode(vehicle.id(), vehicle.compId(), &message, &position);
    gps->handleMessage(&vehicle, message);
    QCOMPARE(cog->rawValue().toDouble(), 0.0);
    attitude.yaw = qQNaN();
    mavlink_msg_attitude_encode(vehicle.id(), vehicle.compId(), &message, &attitude);
    vehicle.VehicleFactGroup::handleMessage(&vehicle, message);
    QVERIFY(qIsNaN(vehicle.heading()->rawValue().toDouble()));
    QCOMPARE(cog->rawValue().toDouble(), 0.0);
}
