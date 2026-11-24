#include "BoatNavigationTest.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>
#include <limits>

#include "Fact.h"
#include "FirmwarePlugin.h"
#include "LinkManager.h"
#include "MissionController.h"
#include "MissionManager.h"
#include "MissionSettingsItem.h"
#include "MockLink.h"
#include "PlanMasterController.h"
#include "SimpleMissionItem.h"
#include "SpeedSection.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

UT_REGISTER_TEST(BoatNavigationTest, TestLabel::Integration, TestLabel::Vehicle)

void BoatNavigationTest::_checkManualControlCount(int expected)
{
    auto* manager = _vehicle->vehicleLinkManager();
    QVERIFY(manager);
    const auto link = manager->primaryLink().lock();
    QVERIFY(link);
    const int previous = _mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PING);
    mavlink_ping_t ping{};
    ping.seq = previous + 1;
    ping.target_system = _vehicle->id();
    ping.target_component = _vehicle->compId();
    mavlink_message_t message{};
    mavlink_msg_ping_encode(255, MAV_COMP_ID_MISSIONPLANNER, &message, &ping);
    // A following packet is a queue barrier for both transmitted and rejected control inputs.
    QVERIFY(_vehicle->sendMessageOnLinkThreadSafe(link.get(), message));
    QTRY_VERIFY_WITH_TIMEOUT(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_PING) > previous,
                             TestTimeout::mediumMs());
    QCOMPARE(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MANUAL_CONTROL), expected);
}

void BoatNavigationTest::_virtualJoystickMapping_data()
{
    QTest::addColumn<int>("type");
    for (const auto type : {MAV_TYPE_SURFACE_BOAT, MAV_TYPE_GROUND_ROVER, MAV_TYPE_QUADROTOR, MAV_TYPE_FIXED_WING,
                            MAV_TYPE_VTOL_TILTROTOR}) {
        QTest::newRow(QStringLiteral("type%1").arg(type).toLatin1().constData()) << int(type);
    }
}

void BoatNavigationTest::_virtualJoystickMapping()
{
    QFETCH(int, type);
    _connectVehicle(static_cast<MAV_TYPE>(type));
    if (QTest::currentTestFailed()) {
        return;
    }
    _vehicle->virtualTabletJoystickValue(0.25, -0.5, 0.75, -1.0);
    _checkManualControlCount(1);
    mavlink_message_t message{};
    QVERIFY(_mockLink->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_MANUAL_CONTROL, message));
    mavlink_manual_control_t control{};
    mavlink_msg_manual_control_decode(&message, &control);
    QCOMPARE(control.target, _vehicle->id());
    QCOMPARE(control.x, -500);
    QCOMPARE(control.y, 250);
    QCOMPARE(control.r, 750);
    QCOMPARE(control.z, -1000);
}

void BoatNavigationTest::_virtualJoystickRejectsInvalidInput()
{
    _connectVehicle(MAV_TYPE_SURFACE_BOAT);
    if (QTest::currentTestFailed()) {
        return;
    }
    for (const double invalid : {qQNaN(), std::numeric_limits<double>::infinity(), -2.0, 2.0}) {
        for (int index = 0; index < 4; ++index) {
            double axes[] = {0.0, 0.0, 0.0, 0.0};
            axes[index] = invalid;
            _vehicle->virtualTabletJoystickValue(axes[0], axes[1], axes[2], axes[3]);
        }
    }
    _checkManualControlCount(0);
    auto* manager = MultiVehicleManager::instance();
    manager->setActiveVehicle(nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(manager->activeVehicle() != _vehicle, TestTimeout::mediumMs());
    _vehicle->virtualTabletJoystickValue(1.0, 1.0, 1.0, 1.0);
    _checkManualControlCount(0);
    manager->setActiveVehicle(_vehicle);
    QTRY_COMPARE_WITH_TIMEOUT(manager->activeVehicle(), _vehicle, TestTimeout::mediumMs());

    _mockLink->setCommLost(true);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->vehicleLinkManager()->communicationLost(), TestTimeout::mediumMs());
    _vehicle->virtualTabletJoystickValue(1.0, 1.0, 1.0, 1.0);
    _mockLink->setCommLost(false);
    _checkManualControlCount(0);
    QTRY_VERIFY_WITH_TIMEOUT(!_vehicle->vehicleLinkManager()->communicationLost(), TestTimeout::mediumMs());
    _vehicle->virtualTabletJoystickValue(0.0, 0.0, 0.0, 0.0);
    _checkManualControlCount(1);
}

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

void BoatNavigationTest::_planSpeedVehicleRoundTrip_data()
{
    _pauseWithoutAltitude_data();
}

void BoatNavigationTest::_planSpeedVehicleRoundTrip()
{
    QFETCH(int, type);
    _connectVehicle(static_cast<MAV_TYPE>(type));
    if (QTest::currentTestFailed()) {
        return;
    }
    PlanMasterController master;
    master.setFlyView(false);
    master.start();
    auto* mission = master.missionController();
    mission->insertSimpleMissionItem(QGeoCoordinate(47, 8, 0), 1, false);
    mission->insertSimpleMissionItem(QGeoCoordinate(47.001, 8.001, 0), 2, false);
    auto* waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    auto* settings = mission->visualItems()->value<MissionSettingsItem*>(0);
    QVERIFY(waypoint && settings);
    const bool speedSupported = type != MAV_TYPE_GROUND_ROVER;
    QCOMPARE(waypoint->speedSection()->available(), speedSupported);
    if (speedSupported) {
        settings->speedSection()->setSpecifyFlightSpeed(true);
        settings->speedSection()->flightSpeed()->setRawValue(2.0);
        waypoint->speedSection()->setSpecifyFlightSpeed(true);
        waypoint->speedSection()->flightSpeed()->setRawValue(3.0);
    }
    QSignalSpy uploaded(_vehicle->missionManager(), &MissionManager::sendComplete);
    master.sendToVehicle();
    QTRY_VERIFY_WITH_TIMEOUT(!uploaded.isEmpty(), TestTimeout::longMs());
    QVERIFY(!uploaded.last().at(0).toBool());
    QVERIFY(!master.syncInProgress());
    QVERIFY(!master.dirtyForUpload());
    mission->loadFromVehicle();
    QTRY_VERIFY_WITH_TIMEOUT(!mission->syncInProgress(), TestTimeout::longMs());
    QCOMPARE(mission->visualItems()->count(), 3);
    settings = mission->visualItems()->value<MissionSettingsItem*>(0);
    waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(settings && waypoint);
    QCOMPARE(settings->speedSection()->specifyFlightSpeed(), speedSupported);
    QCOMPARE(waypoint->speedSection()->specifyFlightSpeed(), speedSupported);
    if (speedSupported) {
        QCOMPARE(settings->speedSection()->specifiedFlightSpeed(), 2.0);
        QCOMPARE(waypoint->speedSection()->specifiedFlightSpeed(), 3.0);
    }
    int speedCount = 0;
    for (const MissionItem* item : _vehicle->missionManager()->missionItems()) {
        if (item->command() == MAV_CMD_DO_CHANGE_SPEED) {
            QCOMPARE(item->param1(), 1.0);
            QCOMPARE(item->param3(), -1.0);
            speedCount++;
        }
    }
    QCOMPARE(speedCount, speedSupported ? 2 : 0);
}

void BoatNavigationTest::_rejectAircraftMission_data()
{
    QTest::addColumn<int>("command");
    for (const auto command :
         {MAV_CMD_NAV_TAKEOFF, MAV_CMD_NAV_LAND, MAV_CMD_NAV_TAKEOFF_LOCAL, MAV_CMD_NAV_LAND_LOCAL,
          MAV_CMD_NAV_VTOL_TAKEOFF, MAV_CMD_NAV_VTOL_LAND, MAV_CMD_NAV_LOITER_TO_ALT,
          MAV_CMD_NAV_CONTINUE_AND_CHANGE_ALT, MAV_CMD_CONDITION_CHANGE_ALT, MAV_CMD_DO_CHANGE_ALTITUDE,
          MAV_CMD_DO_LAND_START, MAV_CMD_DO_GO_AROUND, MAV_CMD_DO_VTOL_TRANSITION}) {
        QTest::newRow(QString::number(command).toLatin1().constData()) << int(command);
    }
}

void BoatNavigationTest::_rejectAircraftMission()
{
    QFETCH(int, command);
    _connectVehicle(MAV_TYPE_SURFACE_BOAT);
    if (QTest::currentTestFailed()) {
        return;
    }
    PlanMasterController master;
    master.setFlyView(false);
    master.start();
    auto* mission = master.missionController();
    mission->insertSimpleMissionItem(QGeoCoordinate(47, 8, 0), 1, false);
    auto* waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(waypoint);
    waypoint->setCommand(command);
    const int originalCommand = waypoint->command();
    QSignalSpy sent(_vehicle->missionManager(), &MissionManager::sendComplete);
    expectAppMessage(QRegularExpression(QStringLiteral("Boat mission contains an aircraft-only command")));
    master.sendToVehicle();
    verifyExpectedLogMessage();
    QVERIFY(!master.syncInProgress());
    QVERIFY(master.dirtyForUpload());
    QVERIFY(mission->dirty());
    QCOMPARE(sent.count(), 0);
    QCOMPARE(waypoint->command(), originalCommand);
    // An explicit correction and retry must still work after the rejected upload.
    waypoint->setCommand(MAV_CMD_NAV_WAYPOINT);
    master.sendToVehicle();
    QTRY_VERIFY_WITH_TIMEOUT(!sent.isEmpty(), TestTimeout::longMs());
    QVERIFY(!sent.last().at(0).toBool());
    QVERIFY(!master.dirtyForUpload());
}
