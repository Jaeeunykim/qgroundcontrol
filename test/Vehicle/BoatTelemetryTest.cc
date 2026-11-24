#include "BoatTelemetryTest.h"

#include <QtCore/QRegularExpression>
#include <QtGui/QQuaternion>
#include <limits>

#include "AppSettings.h"
#include "MultiVehicleManager.h"
#include "SettingsManager.h"
#include "UnitsSettings.h"
#include "Vehicle.h"
#include "VehicleGPSFactGroup.h"

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

void BoatTelemetryTest::_estimateFollowsDistanceChanges()
{
    Vehicle vehicle(MAV_AUTOPILOT_PX4, MAV_TYPE_SURFACE_BOAT);
    sendSpeed(vehicle, 2.0f);
    QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
    vehicle.distanceToHome()->setRawValue(100.0);
    QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 50.0);
    vehicle.distanceToHome()->setRawValue(40.0);
    QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 20.0);
    vehicle.distanceToHome()->setRawValue(qQNaN());
    QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
    vehicle.distanceToHome()->setRawValue(0.0);
    QCOMPARE(vehicle.timeToHome()->rawValue().toDouble(), 0.0);
    sendSpeed(vehicle, 0.0f);
    QVERIFY(qIsNaN(vehicle.timeToHome()->rawValue().toDouble()));
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

void BoatTelemetryTest::_gpsCourseValidity_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("secondary");
    QTest::addColumn<int>("fix");
    QTest::addColumn<int>("course");
    QTest::addColumn<double>("expected");

    struct Sample
    {
        const char* name;
        int fix;
        int course;
        double boat;
        double legacy;
    };

    const QList<Sample> samples{{"no-gps", 0, 0, qQNaN(), 0},
                                {"no-fix", 1, 9000, qQNaN(), 90},
                                {"2d-north", 2, 0, 0, 0},
                                {"3d-last-angle", 3, 35999, 359.99, 359.99},
                                {"rtk", 6, 27000, 270, 270},
                                {"ppp-north", 8, 0, 0, 0},
                                {"unknown-fix", 9, 0, qQNaN(), 0},
                                {"invalid-fix", 255, 9000, qQNaN(), 90},
                                {"360-degrees", 3, 36000, qQNaN(), 360},
                                {"out-of-range", 3, 65534, qQNaN(), 655.34},
                                {"sentinel", 3, UINT16_MAX, qQNaN(), qQNaN()},
                                {"2d-sentinel", 2, UINT16_MAX, qQNaN(), qQNaN()},
                                {"east", 3, 9000, 90, 90}};
    for (int type : {int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_QUADROTOR),
                     int(MAV_TYPE_FIXED_WING), int(MAV_TYPE_VTOL_TILTROTOR)}) {
        for (bool secondary : {false, true}) {
            for (const auto& sample : samples) {
                const auto name =
                    QStringLiteral("type%1-gps%2-%3").arg(type).arg(secondary ? 2 : 1).arg(sample.name).toLatin1();
                QTest::newRow(name.constData()) << type << secondary << sample.fix << sample.course
                                                << (type == MAV_TYPE_SURFACE_BOAT ? sample.boat : sample.legacy);
            }
        }
    }
}

void BoatTelemetryTest::_gpsCourseValidity()
{
    QFETCH(int, type);
    QFETCH(bool, secondary);
    QFETCH(int, fix);
    QFETCH(int, course);
    QFETCH(double, expected);
    Vehicle vehicle(MAV_AUTOPILOT_PX4, static_cast<MAV_TYPE>(type));
    auto* group = secondary ? vehicle.gps2FactGroup() : vehicle.gpsFactGroup();
    auto* other = secondary ? vehicle.gpsFactGroup() : vehicle.gps2FactGroup();
    QVERIFY(group && other);
    auto* cog = group->getFact(QStringLiteral("courseOverGround"));
    QVERIFY(cog);
    vehicle.heading()->setRawValue(180.0);
    const auto receive = [&](int fixType, int rawCourse) {
        mavlink_message_t message{};
        if (secondary) {
            mavlink_gps2_raw_t gps{};
            gps.fix_type = fixType;
            gps.cog = rawCourse;
            mavlink_msg_gps2_raw_encode(vehicle.id(), vehicle.compId(), &message, &gps);
        } else {
            mavlink_gps_raw_int_t gps{};
            gps.fix_type = fixType;
            gps.cog = rawCourse;
            mavlink_msg_gps_raw_int_encode(vehicle.id(), vehicle.compId(), &message, &gps);
        }
        group->handleMessage(&vehicle, message);
    };
    receive(3, 9000);
    QCOMPARE(cog->rawValue().toDouble(), 90.0);
    receive(fix, course);
    if (qIsNaN(expected)) {
        QVERIFY(qIsNaN(cog->rawValue().toDouble()));
    } else {
        QCOMPARE(cog->rawValue().toDouble(), expected);
    }
    QCOMPARE(vehicle.heading()->rawValue().toDouble(), 180.0);
    QVERIFY(qIsNaN(other->getFact(QStringLiteral("courseOverGround"))->rawValue().toDouble()));
    receive(3, 0);
    QCOMPARE(cog->rawValue().toDouble(), 0.0);
}

void BoatTelemetryTest::_gpsCourseWithoutVehicle()
{
    VehicleGPSFactGroup group;
    mavlink_gps_raw_int_t gps{};
    gps.cog = 36000;
    mavlink_message_t message{};
    mavlink_msg_gps_raw_int_encode(1, MAV_COMP_ID_AUTOPILOT1, &message, &gps);
    group.handleMessage(nullptr, message);
    QCOMPARE(group.courseOverGround()->rawValue().toDouble(), 360.0);
}

void BoatTelemetryTest::_quaternionValidity_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QQuaternion>("rotation");
    QTest::addColumn<QQuaternion>("offset");
    QTest::addColumn<double>("expected");

    struct Sample
    {
        const char* name;
        QQuaternion rotation;
        QQuaternion offset;
        double boat;
        double legacy;
    };

    const QQuaternion noOffset(0, 0, 0, 0);
    const QQuaternion identity(1, 0, 0, 0);
    const QList<Sample> samples{
        {"zero", noOffset, noOffset, qQNaN(), 0},
        {"identity", identity, noOffset, 0, 0},
        {"yaw90", QQuaternion(qSqrt(0.5f), 0, 0, qSqrt(0.5f)), noOffset, 90, 90},
        {"scaled-identity", QQuaternion(2, 0, 0, 0), noOffset, 0, 0},
        {"small-yaw90", QQuaternion(1e-15f, 0, 0, 1e-15f), noOffset, 90, 90},
        {"nan", QQuaternion(qQNaN(), 0, 0, 0), noOffset, qQNaN(), qQNaN()},
        {"infinity", QQuaternion(std::numeric_limits<float>::infinity(), 0, 0, 0), noOffset, qQNaN(), qQNaN()},
        {"norm-overflow", QQuaternion(std::numeric_limits<float>::max(), 0, 0, 0), noOffset, qQNaN(), 0},
        {"norm-underflow", QQuaternion(std::numeric_limits<float>::min(), 0, 0, 0), noOffset, qQNaN(), 0},
        {"nan-offset", identity, QQuaternion(qQNaN(), 0, 0, 0), qQNaN(), 0}};
    for (int type : {int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_QUADROTOR),
                     int(MAV_TYPE_FIXED_WING), int(MAV_TYPE_VTOL_TILTROTOR)}) {
        for (const auto& sample : samples) {
            const auto name = QStringLiteral("type%1-%2").arg(type).arg(sample.name).toLatin1();
            QTest::newRow(name.constData()) << type << sample.rotation << sample.offset
                                            << (type == MAV_TYPE_SURFACE_BOAT ? sample.boat : sample.legacy);
        }
    }
}

void BoatTelemetryTest::_quaternionValidity()
{
    QFETCH(int, type);
    QFETCH(QQuaternion, rotation);
    QFETCH(QQuaternion, offset);
    QFETCH(double, expected);
    Vehicle vehicle(MAV_AUTOPILOT_PX4, static_cast<MAV_TYPE>(type));
    auto* course = vehicle.gpsFactGroup()->getFact(QStringLiteral("courseOverGround"));
    QVERIFY(course);
    course->setRawValue(270.0);
    mavlink_attitude_quaternion_t attitude{};
    attitude.q1 = rotation.scalar();
    attitude.q2 = rotation.x();
    attitude.q3 = rotation.y();
    attitude.q4 = rotation.z();
    attitude.repr_offset_q[0] = offset.scalar();
    attitude.repr_offset_q[1] = offset.x();
    attitude.repr_offset_q[2] = offset.y();
    attitude.repr_offset_q[3] = offset.z();
    attitude.rollspeed = 0.1f;
    attitude.pitchspeed = 0.2f;
    attitude.yawspeed = 0.3f;
    mavlink_message_t message{};
    vehicle.heading()->setRawValue(180.0);
    mavlink_msg_attitude_quaternion_encode(vehicle.id(), vehicle.compId(), &message, &attitude);
    vehicle.VehicleFactGroup::handleMessage(&vehicle, message);
    if (qIsNaN(expected)) {
        QVERIFY(qIsNaN(vehicle.heading()->rawValue().toDouble()));
        if (type == MAV_TYPE_SURFACE_BOAT) {
            for (Fact* fact :
                 {vehicle.roll(), vehicle.pitch(), vehicle.rollRate(), vehicle.pitchRate(), vehicle.yawRate()}) {
                QVERIFY(qIsNaN(fact->rawValue().toDouble()));
            }
        }
    } else {
        QVERIFY(qAbs(vehicle.heading()->rawValue().toDouble() - expected) <= 1.0);
    }
    QCOMPARE(course->rawValue().toDouble(), 270.0);

    // A subsequent valid north sample must recover from every invalid representation.
    attitude.q1 = 1;
    attitude.q2 = attitude.q3 = attitude.q4 = 0;
    for (float& value : attitude.repr_offset_q) {
        value = 0;
    }
    mavlink_msg_attitude_quaternion_encode(vehicle.id(), vehicle.compId(), &message, &attitude);
    vehicle.VehicleFactGroup::handleMessage(&vehicle, message);
    QCOMPARE(vehicle.heading()->rawValue().toDouble(), 0.0);
    QVERIFY(qIsFinite(vehicle.yawRate()->rawValue().toDouble()));
}

void BoatTelemetryTest::_attitudeWithoutVehicle()
{
    VehicleFactGroup group;
    mavlink_message_t message{};
    mavlink_attitude_t euler{};
    mavlink_msg_attitude_encode(1, MAV_COMP_ID_AUTOPILOT1, &message, &euler);
    group.handleMessage(nullptr, message);
    mavlink_attitude_quaternion_t quaternion{};
    quaternion.q1 = 1;
    mavlink_msg_attitude_quaternion_encode(1, MAV_COMP_ID_AUTOPILOT1, &message, &quaternion);
    group.handleMessage(nullptr, message);
    QVERIFY(!group.telemetryAvailable());
}
