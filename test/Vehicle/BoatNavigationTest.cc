#include "BoatNavigationTest.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>

#include "Fact.h"
#include "FirmwarePlugin.h"
#include "LinkManager.h"
#include "MockLink.h"
#include "Vehicle.h"

UT_REGISTER_TEST(BoatNavigationTest, TestLabel::Integration, TestLabel::Vehicle)

void BoatNavigationTest::_connectVehicle(MAV_TYPE type, bool commandInt)
{
    auto* mockConfig = new MockConfiguration(QStringLiteral("Boat navigation regression"));
    mockConfig->setDynamic(true);
    mockConfig->setFirmwareType(MAV_AUTOPILOT_PX4);
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
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->coordinate().isValid(), TestTimeout::mediumMs());
    QCOMPARE(_vehicle->vehicleType(), type);
    if (commandInt) {
        _vehicle->_capabilityBits |= MAV_PROTOCOL_CAPABILITY_COMMAND_INT;
    } else {
        _vehicle->_capabilityBits &= ~uint64_t(MAV_PROTOCOL_CAPABILITY_COMMAND_INT);
    }
    _mockLink->setRepositionCommandResponse(MAV_RESULT_ACCEPTED);
    _mockLink->clearReceivedMavCommandCounts();
    _mockLink->clearReceivedMavlinkMessageCounts();
}

bool BoatNavigationTest::_hasResult(const QSignalSpy& spy, MAV_RESULT result) const
{
    for (const auto& args : spy) {
        if (args.at(2).toInt() == MAV_CMD_DO_REPOSITION && args.at(3).toInt() == result) {
            return true;
        }
    }
    return false;
}

void BoatNavigationTest::_gotoLocation_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("knownAltitude");
    QTest::addColumn<bool>("commandInt");
    for (const auto type : {MAV_TYPE_SURFACE_BOAT, MAV_TYPE_GROUND_ROVER, MAV_TYPE_QUADROTOR, MAV_TYPE_FIXED_WING,
                            MAV_TYPE_VTOL_TILTROTOR}) {
        for (const bool known : {false, true}) {
            for (const bool useInt : {false, true}) {
                const QByteArray name =
                    QStringLiteral("type%1-alt%2-int%3").arg(type).arg(known).arg(useInt).toLatin1();
                QTest::newRow(name.constData()) << int(type) << known << useInt;
            }
        }
    }
}

void BoatNavigationTest::_gotoLocation()
{
    QFETCH(int, type);
    QFETCH(bool, knownAltitude);
    QFETCH(bool, commandInt);
    _connectVehicle(static_cast<MAV_TYPE>(type), commandInt);
    if (QTest::currentTestFailed()) {
        return;
    }
    _vehicle->altitudeAMSL()->setRawValue(knownAltitude ? 410.0 : qQNaN());
    QGeoCoordinate target = _vehicle->coordinate();
    target.setLatitude(target.latitude() + 0.0001);
    const bool expected = type == MAV_TYPE_SURFACE_BOAT || knownAltitude;
    if (!expected) {
        expectAppMessage(QRegularExpression(QStringLiteral("vehicle position not known")));
    }
    QSignalSpy result(_vehicle, &Vehicle::mavCommandResult);
    QCOMPARE(_vehicle->guidedModeGotoLocation(target), expected);
    if (!expected) {
        verifyExpectedLogMessage();
        QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_DO_REPOSITION), 0);
        return;
    }
    QTRY_VERIFY_WITH_TIMEOUT(_hasResult(result, MAV_RESULT_ACCEPTED), TestTimeout::mediumMs());
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_DO_REPOSITION), 1);
    mavlink_message_t message{};
    float altitude = 0.0f;
    if (commandInt) {
        QVERIFY(_mockLink->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_COMMAND_INT, message));
        mavlink_command_int_t command{};
        mavlink_msg_command_int_decode(&message, &command);
        QCOMPARE(command.command, uint16_t(MAV_CMD_DO_REPOSITION));
        QCOMPARE(command.frame, uint8_t(MAV_FRAME_GLOBAL));
        QVERIFY(qAbs(command.x / 1e7 - target.latitude()) < 1e-6);
        QVERIFY(qAbs(command.y / 1e7 - target.longitude()) < 1e-6);
        QCOMPARE(command.param1, -1.0f);
        QCOMPARE(command.param2, float(MAV_DO_REPOSITION_FLAGS_CHANGE_MODE));
        QVERIFY(qIsNaN(command.param4));
        altitude = command.z;
    } else {
        QVERIFY(_mockLink->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_COMMAND_LONG, message));
        mavlink_command_long_t command{};
        mavlink_msg_command_long_decode(&message, &command);
        QCOMPARE(command.command, uint16_t(MAV_CMD_DO_REPOSITION));
        QVERIFY(qAbs(command.param5 - target.latitude()) < 1e-5);
        QVERIFY(qAbs(command.param6 - target.longitude()) < 1e-5);
        QCOMPARE(command.param1, -1.0f);
        QCOMPARE(command.param2, float(MAV_DO_REPOSITION_FLAGS_CHANGE_MODE));
        QVERIFY(qIsNaN(command.param4));
        altitude = command.param7;
    }
    if (type == MAV_TYPE_SURFACE_BOAT) {
        QVERIFY(qIsNaN(altitude));
    } else {
        QCOMPARE(altitude, 410.0f);
    }
}

void BoatNavigationTest::_pauseWithoutAltitude_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("boat") << int(MAV_TYPE_SURFACE_BOAT);
    QTest::newRow("rover") << int(MAV_TYPE_GROUND_ROVER);
    QTest::newRow("multirotor") << int(MAV_TYPE_QUADROTOR);
}

void BoatNavigationTest::_pauseWithoutAltitude()
{
    QFETCH(int, type);
    _connectVehicle(static_cast<MAV_TYPE>(type));
    if (QTest::currentTestFailed()) {
        return;
    }
    _vehicle->altitudeAMSL()->setRawValue(qQNaN());
    _vehicle->altitudeRelative()->setRawValue(qQNaN());
    QSignalSpy result(_vehicle, &Vehicle::mavCommandResult);
    _vehicle->pauseVehicle();
    QTRY_VERIFY_WITH_TIMEOUT(_hasResult(result, MAV_RESULT_ACCEPTED), TestTimeout::mediumMs());
    mavlink_message_t message{};
    QVERIFY(_mockLink->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_COMMAND_LONG, message));
    mavlink_command_long_t command{};
    mavlink_msg_command_long_decode(&message, &command);
    QCOMPARE(command.command, uint16_t(MAV_CMD_DO_REPOSITION));
    QVERIFY(qIsNaN(command.param4));
    QVERIFY(qIsNaN(command.param5));
    QVERIFY(qIsNaN(command.param6));
    QVERIFY(qIsNaN(command.param7));
}

void BoatNavigationTest::_rejectAircraftActions_data()
{
    QTest::addColumn<int>("action");
    QTest::newRow("land") << 0;
    QTest::newRow("guided-takeoff") << 1;
    QTest::newRow("start-takeoff") << 2;
    QTest::newRow("change-altitude") << 3;
    QTest::newRow("abort-landing") << 4;
}

void BoatNavigationTest::_rejectAircraftActions()
{
    QFETCH(int, action);
    _connectVehicle(MAV_TYPE_SURFACE_BOAT);
    if (QTest::currentTestFailed()) {
        return;
    }
    expectAppMessage(QRegularExpression(QStringLiteral("not supported for boats")));
    switch (action) {
        case 0:
            _vehicle->guidedModeLand();
            break;
        case 1:
            _vehicle->guidedModeTakeoff(10.0);
            break;
        case 2:
            _vehicle->startTakeoff();
            break;
        case 3:
            _vehicle->guidedModeChangeAltitude(10.0, true);
            break;
        case 4:
            _vehicle->abortLanding(10.0);
            break;
    }
    verifyExpectedLogMessage();
    const auto marker = MockLink::MAV_CMD_MOCKLINK_ALWAYS_RESULT_ACCEPTED;
    _vehicle->sendMavCommand(_vehicle->defaultComponentId(), marker, false);
    QTRY_COMPARE_WITH_TIMEOUT(_mockLink->receivedMavCommandCount(marker), 1, TestTimeout::mediumMs());
    for (const auto command : {MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_LAND, MAV_CMD_DO_REPOSITION, MAV_CMD_DO_GO_AROUND,
                               MAV_CMD_COMPONENT_ARM_DISARM, MAV_CMD_DO_SET_MODE}) {
        QCOMPARE(_mockLink->receivedMavCommandCount(command), 0);
    }
}

void BoatNavigationTest::_missionAndReturn_data()
{
    _pauseWithoutAltitude_data();
}

void BoatNavigationTest::_missionAndReturn()
{
    QFETCH(int, type);
    _connectVehicle(static_cast<MAV_TYPE>(type));
    if (QTest::currentTestFailed()) {
        return;
    }
    _vehicle->startMission();
    QVERIFY(_vehicle->armed());
    QCOMPARE(_vehicle->flightMode(), _vehicle->firmwarePlugin()->missionFlightMode());
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_NAV_TAKEOFF), 0);
    _vehicle->guidedModeRTL(false);
    QCOMPARE(_vehicle->flightMode(), _vehicle->firmwarePlugin()->rtlFlightMode());
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_NAV_LAND), 0);
}

void BoatNavigationTest::_commandResults_data()
{
    QTest::addColumn<int>("resultCode");
    QTest::addColumn<bool>("respond");
    QTest::addColumn<bool>("inProgress");
    QTest::newRow("accepted") << int(MAV_RESULT_ACCEPTED) << true << false;
    QTest::newRow("denied") << int(MAV_RESULT_DENIED) << true << false;
    QTest::newRow("unsupported") << int(MAV_RESULT_UNSUPPORTED) << true << false;
    QTest::newRow("timeout") << int(MAV_RESULT_FAILED) << false << false;
    QTest::newRow("progress-accepted") << int(MAV_RESULT_ACCEPTED) << true << true;
    QTest::newRow("progress-failed") << int(MAV_RESULT_FAILED) << true << true;
}

void BoatNavigationTest::_commandResults()
{
    QFETCH(int, resultCode);
    QFETCH(bool, respond);
    QFETCH(bool, inProgress);
    _connectVehicle(MAV_TYPE_SURFACE_BOAT);
    if (QTest::currentTestFailed()) {
        return;
    }
    _mockLink->setRepositionCommandResponse(inProgress ? MAV_RESULT_IN_PROGRESS : static_cast<MAV_RESULT>(resultCode),
                                            respond);
    const QString modeBefore = _vehicle->flightMode();
    if (resultCode != MAV_RESULT_ACCEPTED) {
        const QString message = !respond                               ? QStringLiteral("did not respond to command")
                                : resultCode == MAV_RESULT_DENIED      ? QStringLiteral("command denied")
                                : resultCode == MAV_RESULT_UNSUPPORTED ? QStringLiteral("command not supported")
                                                                       : QStringLiteral("command failed");
        expectAppMessage(QRegularExpression(message));
    }
    if (!respond) {
        ignoreLogMessage("Vehicle.MavCommandQueue", QtWarningMsg,
                         QRegularExpression(QStringLiteral("Giving up sending command after max retries:")));
    }
    QSignalSpy result(_vehicle, &Vehicle::mavCommandResult);
    QVERIFY(_vehicle->guidedModeGotoLocation(_vehicle->coordinate()));
    if (inProgress) {
        QTRY_VERIFY_WITH_TIMEOUT(_mockLink->receivedMavCommandCount(MAV_CMD_DO_REPOSITION) >= 1,
                                 TestTimeout::mediumMs());
        QVERIFY(!_hasResult(result, MAV_RESULT_ACCEPTED));
        _mockLink->sendUnexpectedCommandAck(MAV_CMD_DO_REPOSITION, static_cast<MAV_RESULT>(resultCode));
    }
    QTRY_VERIFY_WITH_TIMEOUT(_hasResult(result, static_cast<MAV_RESULT>(resultCode)), TestTimeout::longMs());
    if (resultCode != MAV_RESULT_ACCEPTED) {
        verifyExpectedLogMessage();
    }
    // An accepted command is not a report that the maneuver or mode transition completed.
    QCOMPARE(_vehicle->flightMode(), modeBefore);
    if (!respond) {
        QCOMPARE(result.last().at(4).toInt(), int(Vehicle::MavCmdResultFailureNoResponseToCommand));
    }
}

void BoatNavigationTest::_invalidGotoCoordinates()
{
    _connectVehicle(MAV_TYPE_SURFACE_BOAT);
    if (QTest::currentTestFailed()) {
        return;
    }
    QVERIFY(!_vehicle->guidedModeGotoLocation(QGeoCoordinate()));
    const QGeoCoordinate valid = _vehicle->coordinate();
    _vehicle->_coordinate = QGeoCoordinate();
    QVERIFY(!_vehicle->guidedModeGotoLocation(valid));
    _vehicle->_coordinate = valid;
    expectAppMessage(QRegularExpression(QStringLiteral("New location is too far")));
    QVERIFY(!_vehicle->guidedModeGotoLocation(QGeoCoordinate(0, 0)));
    verifyExpectedLogMessage();
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_DO_REPOSITION), 0);
}
