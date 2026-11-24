#include "BoatLinkRecoveryTest.h"

#include <QtCore/QRegularExpression>

#include "LinkManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

UT_REGISTER_TEST(BoatLinkRecoveryTest, TestLabel::Integration, TestLabel::Vehicle)

void BoatLinkRecoveryTest::_connectVehicle(MAV_AUTOPILOT autopilot, MAV_TYPE type)
{
    auto* mockConfig = new MockConfiguration(QStringLiteral("Boat link recovery regression"));
    mockConfig->setDynamic(true);
    mockConfig->setFirmwareType(autopilot);
    mockConfig->setVehicleType(type);
    mockConfig->setSendStatusText(false);
    mockConfig->setEnableCamera(false);
    mockConfig->setEnableGimbal(false);
    mockConfig->setEnableProximity(false);
    auto config = LinkManager::instance()->addConfiguration(mockConfig);
    QVERIFY(LinkManager::instance()->createConnectedLink(config));
    _mockLink = qobject_cast<MockLink*>(config->link());
    QVERIFY(_mockLink);
    connect(_mockLink, &QObject::destroyed, this, [this]() { _mockLink = nullptr; });
    QTRY_VERIFY_WITH_TIMEOUT(MultiVehicleManager::instance()->activeVehicle(), TestTimeout::longMs());
    _vehicle = MultiVehicleManager::instance()->activeVehicle();
    QVERIFY(_vehicle);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->isInitialConnectComplete(), TestTimeout::longMs());
    QCOMPARE(_vehicle->vehicleType(), type);
}

bool BoatLinkRecoveryTest::_receive(const mavlink_message_t& message)
{
    return QMetaObject::invokeMethod(_vehicle, "_mavlinkMessageReceived", Qt::DirectConnection,
                                     Q_ARG(LinkInterface*, _mockLink), Q_ARG(mavlink_message_t, message));
}

QList<Fact*> BoatLinkRecoveryTest::_instruments() const
{
    return {_vehicle->groundSpeed(),
            _vehicle->heading(),
            _vehicle->gpsFactGroup()->getFact(QStringLiteral("courseOverGround")),
            _vehicle->distanceToHome(),
            _vehicle->distanceToNextWP(),
            _vehicle->timeToHome()};
}

void BoatLinkRecoveryTest::_lossAndRecovery_data()
{
    QTest::addColumn<int>("autopilot");
    QTest::addColumn<int>("type");
    for (const auto autopilot : {MAV_AUTOPILOT_PX4, MAV_AUTOPILOT_ARDUPILOTMEGA}) {
        for (const auto type : {MAV_TYPE_SURFACE_BOAT, MAV_TYPE_GROUND_ROVER, MAV_TYPE_QUADROTOR, MAV_TYPE_FIXED_WING,
                                MAV_TYPE_VTOL_TILTROTOR}) {
            const QByteArray name = QStringLiteral("autopilot%1-type%2").arg(autopilot).arg(type).toLatin1();
            QTest::newRow(name.constData()) << int(autopilot) << int(type);
        }
    }
}

void BoatLinkRecoveryTest::_lossAndRecovery()
{
    QFETCH(int, autopilot);
    QFETCH(int, type);
    // Discovery can still be pending when this test deliberately silences every link.
    ignoreLogMessage(
        "Vehicle.MavCommandQueue", QtWarningMsg,
        QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                          "MAV_CMD_REQUEST_MESSAGE requesting: GIMBAL_MANAGER_INFORMATION")));
    _connectVehicle(static_cast<MAV_AUTOPILOT>(autopilot), static_cast<MAV_TYPE>(type));
    if (QTest::currentTestFailed()) {
        return;
    }
    auto* linkManager = _vehicle->vehicleLinkManager();
    QVERIFY(linkManager);
    QVERIFY(linkManager->setProperty("autoDisconnect", false));
    _mockLink->setCommLost(true);
    QTRY_VERIFY_WITH_TIMEOUT(linkManager->communicationLost(), TestTimeout::mediumMs());

    mavlink_message_t message{};
    mavlink_home_position_t home{};
    home.latitude = 371010000;
    home.longitude = 1266000000;
    mavlink_msg_home_position_encode(_vehicle->id(), _vehicle->compId(), &message, &home);
    QVERIFY(_receive(message));
    QVERIFY(!linkManager->communicationLost());

    mavlink_global_position_int_t position{};
    position.lat = 371000000;
    position.lon = 1266000000;
    position.alt = 1000;
    mavlink_msg_global_position_int_encode(_vehicle->id(), _vehicle->compId(), &message, &position);
    QVERIFY(_receive(message));
    const auto lastCoordinate = _vehicle->coordinate();

    mavlink_attitude_t attitude{};
    mavlink_attitude_quaternion_t quaternion{};
    quaternion.q1 = qSqrt(0.5f);
    quaternion.q4 = qSqrt(0.5f);
    mavlink_msg_attitude_quaternion_encode(_vehicle->id(), _vehicle->compId(), &message, &quaternion);
    QVERIFY(_receive(message));
    mavlink_gps_raw_int_t gps{};
    gps.fix_type = GPS_FIX_TYPE_3D_FIX;
    gps.lat = position.lat;
    gps.lon = position.lon;
    gps.alt = position.alt;
    gps.cog = 27000;
    mavlink_msg_gps_raw_int_encode(_vehicle->id(), _vehicle->compId(), &message, &gps);
    QVERIFY(_receive(message));
    mavlink_nav_controller_output_t nav{};
    nav.wp_dist = 50;
    mavlink_msg_nav_controller_output_encode(_vehicle->id(), _vehicle->compId(), &message, &nav);
    QVERIFY(_receive(message));
    mavlink_vfr_hud_t hud{};
    hud.groundspeed = 2;
    mavlink_msg_vfr_hud_encode(_vehicle->id(), _vehicle->compId(), &message, &hud);
    QVERIFY(_receive(message));
    for (Fact* fact : _instruments()) {
        QVERIFY(qIsFinite(fact->rawValue().toDouble()));
    }

    QTRY_VERIFY_WITH_TIMEOUT(linkManager->communicationLost(), TestTimeout::mediumMs());
    QCOMPARE(_vehicle->coordinate(), lastCoordinate);
    for (Fact* fact : _instruments()) {
        QCOMPARE(qIsNaN(fact->rawValue().toDouble()), type == MAV_TYPE_SURFACE_BOAT);
    }

    mavlink_heartbeat_t heartbeat{};
    heartbeat.type = type;
    heartbeat.autopilot = autopilot;
    mavlink_msg_heartbeat_encode(_vehicle->id(), _vehicle->compId(), &message, &heartbeat);
    QVERIFY(_receive(message));
    QVERIFY(!linkManager->communicationLost());
    if (type != MAV_TYPE_SURFACE_BOAT) {
        for (Fact* fact : _instruments()) {
            QVERIFY(qIsFinite(fact->rawValue().toDouble()));
        }
        return;
    }
    for (Fact* fact : _instruments()) {
        QVERIFY(qIsNaN(fact->rawValue().toDouble()));
    }

    // A Home update and fresh speed must not revive the pre-loss position or heading.
    home.latitude += 1000;
    mavlink_msg_home_position_encode(_vehicle->id(), _vehicle->compId(), &message, &home);
    QVERIFY(_receive(message));
    hud.groundspeed = 4;
    mavlink_msg_vfr_hud_encode(_vehicle->id(), _vehicle->compId(), &message, &hud);
    QVERIFY(_receive(message));
    QCOMPARE(_vehicle->groundSpeed()->rawValue().toDouble(), 4.0);
    QVERIFY(qIsNaN(_vehicle->distanceToHome()->rawValue().toDouble()));
    QVERIFY(qIsNaN(_vehicle->timeToHome()->rawValue().toDouble()));
    QVERIFY(qIsNaN(_vehicle->heading()->rawValue().toDouble()));

    // A newly received position is valid even when the boat has not moved.
    mavlink_msg_global_position_int_encode(_vehicle->id(), _vehicle->compId(), &message, &position);
    QVERIFY(_receive(message));
    QVERIFY(_vehicle->distanceToHome()->rawValue().toDouble() > 100.0);
    QCOMPARE(_vehicle->timeToHome()->rawValue().toDouble(), _vehicle->distanceToHome()->rawValue().toDouble() / 4.0);
    QVERIFY(qIsNaN(_vehicle->distanceToNextWP()->rawValue().toDouble()));

    attitude.yaw = 0;
    mavlink_msg_attitude_encode(_vehicle->id(), _vehicle->compId(), &message, &attitude);
    QVERIFY(_receive(message));
    QCOMPARE(_vehicle->heading()->rawValue().toDouble(), 0.0);
    QVERIFY(qIsNaN(_vehicle->gpsFactGroup()->getFact(QStringLiteral("courseOverGround"))->rawValue().toDouble()));
    gps.cog = 0;
    mavlink_msg_gps_raw_int_encode(_vehicle->id(), _vehicle->compId(), &message, &gps);
    QVERIFY(_receive(message));
    QCOMPARE(_vehicle->gpsFactGroup()->getFact(QStringLiteral("courseOverGround"))->rawValue().toDouble(), 0.0);
    nav.wp_dist = 0;
    mavlink_msg_nav_controller_output_encode(_vehicle->id(), _vehicle->compId(), &message, &nav);
    QVERIFY(_receive(message));
    QCOMPARE(_vehicle->distanceToNextWP()->rawValue().toDouble(), 0.0);

    // Source preferences are learned again after loss; a GPS-only recovery remains usable.
    QTRY_VERIFY_WITH_TIMEOUT(linkManager->communicationLost(), TestTimeout::mediumMs());
    gps.fix_type = GPS_FIX_TYPE_2D_FIX;
    mavlink_msg_gps_raw_int_encode(_vehicle->id(), _vehicle->compId(), &message, &gps);
    QVERIFY(_receive(message));
    QVERIFY(qIsNaN(_vehicle->distanceToHome()->rawValue().toDouble()));
    gps.fix_type = GPS_FIX_TYPE_3D_FIX;
    mavlink_msg_gps_raw_int_encode(_vehicle->id(), _vehicle->compId(), &message, &gps);
    QVERIFY(_receive(message));
    QVERIFY(_vehicle->distanceToHome()->rawValue().toDouble() > 100.0);
    QVERIFY(qIsNaN(_vehicle->timeToHome()->rawValue().toDouble()));
}
