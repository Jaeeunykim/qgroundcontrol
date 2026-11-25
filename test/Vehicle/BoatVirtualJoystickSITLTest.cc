#include "BoatVirtualJoystickSITLTest.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtQml/QQmlApplicationEngine>

#include "LinkInterface.h"
#include "LinkManager.h"
#include "MAVLinkLib.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "QGCCorePlugin.h"
#include "QmlObjectListModel.h"
#include "UDPLink.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

UT_REGISTER_TEST_STANDALONE(BoatVirtualJoystickSITLTest, TestLabel::Integration, TestLabel::Vehicle, TestLabel::Network)

bool BoatVirtualJoystickSITLTest::_writeReport() const
{
    if (_reportPath.isEmpty()) {
        return false;
    }
    QSaveFile file(_reportPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QJsonObject report{{QStringLiteral("completed"), _completed},
                             {QStringLiteral("current_phase"), _phase},
                             {QStringLiteral("phases"), _phases},
                             {QStringLiteral("armed"), _vehicle ? _vehicle->armed() : false},
                             {QStringLiteral("flight_mode"), _vehicle ? _vehicle->flightMode() : QString()}};
    if (file.write(QJsonDocument(report).toJson()) < 0) {
        return false;
    }
    return file.commit();
}

bool BoatVirtualJoystickSITLTest::_hold(double throttle, int simulationMs, const QString& phase, bool armed)
{
    if (_vehicle == nullptr || _vehicle->armed() != armed) {
        return false;
    }
    _phase = phase;
    _throttle = throttle;
    _expectedWireThrottle = qRound((throttle + 1.0) * 500.0);
    _receivedExpectedThrottle = false;
    _unexpectedThrottleAfterExpected = false;
    _wireThrottle = QJsonArray();
    const auto start = QDateTime::currentMSecsSinceEpoch();
    const auto simulationStart = _simulationMs;
    const auto coordinateStart = _vehicle->coordinate();
    const auto headingStart = _vehicle->heading()->rawValue().toDouble();
    _positions.clear();
    if (!_writeReport()) {
        return false;
    }
    // Actuation duration follows PX4's simulated clock, not the host's rendering speed.
    const bool finished = QTest::qWaitFor(
        [&]() {
            return _vehicle == nullptr || _vehicle->armed() != armed ||
                   _vehicle->vehicleLinkManager()->communicationLost() ||
                   (_simulationMs >= simulationStart &&
                    _simulationMs - simulationStart >= static_cast<quint64>(simulationMs));
        },
        (simulationMs * 10) + 10000);
    if (!finished || _vehicle == nullptr || _vehicle->armed() != armed ||
        _vehicle->vehicleLinkManager()->communicationLost() || _positions.isEmpty()) {
        return false;
    }
    const auto coordinateEnd = _vehicle->coordinate();
    const auto tail = _positions.lowerBound(_simulationMs > 3000 ? _simulationMs - 3000 : 0);
    const auto tailStart = tail == _positions.cend() ? _positions.first() : tail.value();
    const double projection = coordinateStart.distanceTo(coordinateEnd) *
                              qCos(qDegreesToRadians(coordinateStart.azimuthTo(coordinateEnd) - headingStart));
    const double tailProjection = tailStart.distanceTo(coordinateEnd) *
                                  qCos(qDegreesToRadians(tailStart.azimuthTo(coordinateEnd) - headingStart));
    const QJsonObject sample{
        {QStringLiteral("name"), phase},
        {QStringLiteral("normalized_throttle"), throttle},
        {QStringLiteral("wire_throttle_values"), _wireThrottle},
        {QStringLiteral("stable_wire_throttle"), _receivedExpectedThrottle && !_unexpectedThrottleAfterExpected},
        {QStringLiteral("start_wall_ms"), start},
        {QStringLiteral("end_wall_ms"), QDateTime::currentMSecsSinceEpoch()},
        {QStringLiteral("start_sim_ms"), static_cast<qint64>(simulationStart)},
        {QStringLiteral("end_sim_ms"), static_cast<qint64>(_simulationMs)},
        {QStringLiteral("heading_start_deg"), headingStart},
        {QStringLiteral("distance_m"), coordinateStart.distanceTo(coordinateEnd)},
        {QStringLiteral("azimuth_deg"), coordinateStart.azimuthTo(coordinateEnd)},
        {QStringLiteral("signed_distance_m"), projection},
        {QStringLiteral("tail_signed_distance_m"), tailProjection},
        {QStringLiteral("end_ground_speed_mps"), _vehicle->groundSpeed()->rawValue().toDouble()}};
    _phases.append(sample);
    return _writeReport() && _receivedExpectedThrottle && !_unexpectedThrottleAfterExpected;
}

void BoatVirtualJoystickSITLTest::_neutralForwardReverse()
{
    if (qEnvironmentVariable("QGC_BOAT_SITL_ARMING") != QStringLiteral("I_CONFIRM_SIMULATION")) {
        QSKIP("Live actuation requires an explicitly confirmed simulator endpoint; see P4_NEUTRAL_TESTING.md.");
    }
    const auto host = qEnvironmentVariable("QGC_BOAT_SITL_HOST");
    const int localPort = qEnvironmentVariableIntValue("QGC_BOAT_SITL_LOCAL_PORT");
    const int remotePort = qEnvironmentVariableIntValue("QGC_BOAT_SITL_REMOTE_PORT");
    const int systemId = qEnvironmentVariableIntValue("QGC_BOAT_SITL_SYSTEM_ID");
    _reportPath = qEnvironmentVariable("QGC_BOAT_SITL_REPORT");
    QVERIFY(!host.isEmpty() && localPort > 0 && localPort <= 65535 && remotePort > 0 && remotePort <= 65535);
    QVERIFY(systemId > 0 && systemId < 255 && !_reportPath.isEmpty());
    ignoreLogMessage(
        "Vehicle.MavCommandQueue", QtWarningMsg,
        QRegularExpression(QStringLiteral("Giving up sending command after max retries: "
                                          "MAV_CMD_REQUEST_MESSAGE requesting: GIMBAL_MANAGER_INFORMATION")));
    // A desktop without a barometer still exercises the full PX4 control path.
    // Ignore only the two host pressure-sensor availability warnings, not vehicle/prearm errors.
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg,
                     QRegularExpression(QStringLiteral("(Failed to connect to pressure backend|"
                                                       "Error Initializing Pressure Sensor)$")));
    // Real PX4 actuator metadata registers an image provider on the application's QML engine.
    // MockLink does not exercise that path; provide the same engine as the UI test fixture.
    _engine = QGCCorePlugin::instance()->createQmlApplicationEngine(this);
    QVERIFY(_engine != nullptr);
    qgcApp()->setQmlAppEngine(_engine);
    MultiVehicleManager::instance()->init();
    LinkManager::instance()->setConnectionsAllowed();
    auto* configuration = new UDPConfiguration(QStringLiteral("Explicit Boat SITL test"));
    configuration->setDynamic(true);
    configuration->setLocalPort(localPort);
    configuration->addHost(host, remotePort);
    _config = LinkManager::instance()->addConfiguration(configuration);
    QVERIFY(LinkManager::instance()->createConnectedLink(_config));
    QTRY_VERIFY_WITH_TIMEOUT(MultiVehicleManager::instance()->activeVehicle(), TestTimeout::longMs());
    _vehicle = MultiVehicleManager::instance()->activeVehicle();
    QVERIFY(_vehicle != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->isInitialConnectComplete(), TestTimeout::longMs());
    QCOMPARE(MultiVehicleManager::instance()->vehicles()->count(), 1);
    QCOMPARE(_vehicle->id(), systemId);
    QCOMPARE(_vehicle->firmwareType(), MAV_AUTOPILOT_PX4);
    QVERIFY(_vehicle->boat());
    QVERIFY2(!_vehicle->armed(), "The confirmed simulator must start disarmed.");
    _authorizedVehicle = true;
    _previousMode = _vehicle->flightMode();
    connect(_vehicle, &Vehicle::mavlinkMessageReceived, this, [this](const mavlink_message_t& message) {
        if (message.msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT) {
            mavlink_global_position_int_t position{};
            mavlink_msg_global_position_int_decode(&message, &position);
            _simulationMs = position.time_boot_ms;
            _positions.insert(_simulationMs, QGeoCoordinate(position.lat / 1e7, position.lon / 1e7));
        }
    });
    QTRY_VERIFY_WITH_TIMEOUT(_simulationMs > 0, TestTimeout::longMs());
    const auto link = _vehicle->vehicleLinkManager()->primaryLink().lock();
    QVERIFY(link != nullptr);
    connect(link.get(), &LinkInterface::bytesSent, this, [this](LinkInterface*, const QByteArray& bytes) {
        for (const char byte : bytes) {
            mavlink_message_t message{};
            mavlink_status_t status{};
            if (mavlink_frame_char_buffer(&_wireMessage, &_wireStatus, static_cast<uint8_t>(byte), &message, &status) ==
                    MAVLINK_FRAMING_OK &&
                message.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
                mavlink_manual_control_t control{};
                mavlink_msg_manual_control_decode(&message, &control);
                // A queued bytesSent notification may still describe the previous phase.
                // Once the new value appears, every subsequent sample must keep that value.
                if (control.z == _expectedWireThrottle) {
                    _receivedExpectedThrottle = true;
                } else if (_receivedExpectedThrottle) {
                    _unexpectedThrottleAfterExpected = true;
                }
                if (!_wireThrottle.contains(static_cast<int>(control.z))) {
                    _wireThrottle.append(static_cast<int>(control.z));
                }
            }
        }
    });
    _sender.setInterval(40);
    connect(&_sender, &QTimer::timeout, this, [this]() {
        if (_vehicle != nullptr) {
            _vehicle->virtualTabletJoystickValue(0, 0, 0, _throttle);
        }
    });
    _sender.start();
    _vehicle->setFlightMode(QStringLiteral("Manual"));
    QTRY_COMPARE_WITH_TIMEOUT(_vehicle->flightMode(), QStringLiteral("Manual"), TestTimeout::longMs());
    QVERIFY(_hold(0, 2000, QStringLiteral("prearm_neutral"), false));
    _vehicle->setArmed(true, true);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->armed(), TestTimeout::longMs());
    QVERIFY(_hold(0, 4000, QStringLiteral("neutral")));
    QVERIFY(_hold(0.4, 6000, QStringLiteral("forward")));
    QVERIFY(_phases.last().toObject().value(QStringLiteral("signed_distance_m")).toDouble() > 0.2);
    QVERIFY(_hold(0, 10000, QStringLiteral("neutral_after_forward")));
    QVERIFY(_hold(-0.4, 18000, QStringLiteral("reverse")));
    QVERIFY(_phases.last().toObject().value(QStringLiteral("tail_signed_distance_m")).toDouble() < -0.1);
    QVERIFY(_hold(0, 4000, QStringLiteral("neutral_after_reverse")));
    _completed = true;
}

void BoatVirtualJoystickSITLTest::cleanup()
{
    bool disarmed = true;
    if (_authorizedVehicle && _vehicle != nullptr) {
        _throttle = 0;
        _vehicle->setArmed(false, true);
        disarmed =
            QTest::qWaitFor([this]() { return _vehicle != nullptr && !_vehicle->armed(); }, TestTimeout::longMs());
        if (disarmed && !_previousMode.isEmpty()) {
            _vehicle->setFlightMode(_previousMode);
            disarmed =
                QTest::qWaitFor([this]() { return _vehicle != nullptr && _vehicle->flightMode() == _previousMode; },
                                TestTimeout::longMs());
        }
        _completed = _completed && disarmed && !QTest::currentTestFailed();
        (void) _writeReport();
    }
    _sender.stop();
    if (_config) {
        LinkManager::instance()->disconnectLinkConfiguration(_config.get());
    }
    const bool disconnected = QTest::qWaitFor(
        []() { return MultiVehicleManager::instance()->vehicles()->count() == 0; }, TestTimeout::longMs());
    if (_engine != nullptr) {
        qgcApp()->setQmlAppEngine(nullptr);
        QGCCorePlugin::instance()->destroyQmlApplicationEngine(_engine);
        _engine = nullptr;
    }
    UnitTest::cleanup();
    QVERIFY2(disarmed, "Simulator disarm was not confirmed.");
    QVERIFY2(disconnected, "Simulator link teardown did not complete.");
}
