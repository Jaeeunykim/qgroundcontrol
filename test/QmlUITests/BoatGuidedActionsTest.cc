#include "BoatGuidedActionsTest.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QVariant>
#include <QtPositioning/QGeoCoordinate>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>

#include "ColoredSvgImageProvider.h"

UT_REGISTER_TEST(BoatGuidedActionsTest, TestLabel::Integration, TestLabel::Vehicle)

BoatGuidedActionsTest::~BoatGuidedActionsTest() = default;

void BoatGuidedActionsTest::initTestCase()
{
    UnitTest::initTestCase();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void BoatGuidedActionsTest::init()
{
    UnitTest::init();
    ignoreLogMessage("qt.qpa.fonts", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Populating font family aliases")));
    _engine = std::make_unique<QQmlEngine>();
    _engine->addImportPath(QStringLiteral("qrc:/qml"));
    _engine->addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(_engine.get(), QUrl(QStringLiteral("qrc:/unittest/BoatGuidedActionsHarness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    _harness.reset(component.create());
    QVERIFY2(_harness, qPrintable(component.errorString()));
    QVERIFY(_part("controller"));
    QVERIFY(_part("vehicleA"));
    QVERIFY(_part("dialog"));
}

void BoatGuidedActionsTest::cleanup()
{
    _harness.reset();
    _engine.reset();
    UnitTest::cleanup();
}

QObject* BoatGuidedActionsTest::_part(const char* name) const
{
    return _harness ? _harness->property(name).value<QObject*>() : nullptr;
}

int BoatGuidedActionsTest::_action(const char* name) const
{
    return _part("controller")->property(name).toInt();
}

bool BoatGuidedActionsTest::_execute(const char* action, QObject* target, bool explicitTarget)
{
    QVariant result;
    const QVariant data = QVariant::fromValue(QGeoCoordinate(47.001, 8.001));
    if (explicitTarget) {
        const bool invoked =
            QMetaObject::invokeMethod(_part("controller"), "executeConfirmedAction", Q_RETURN_ARG(QVariant, result),
                                      Q_ARG(QVariant, _action(action)), Q_ARG(QVariant, data), Q_ARG(QVariant, 50),
                                      Q_ARG(QVariant, false), Q_ARG(QVariant, QVariant::fromValue(target)));
        return invoked && result.toBool();
    }
    const bool invoked = QMetaObject::invokeMethod(_part("controller"), "executeAction", Q_RETURN_ARG(QVariant, result),
                                                   Q_ARG(QVariant, _action(action)), Q_ARG(QVariant, data),
                                                   Q_ARG(QVariant, 50), Q_ARG(QVariant, false));
    return invoked && result.toBool();
}

void BoatGuidedActionsTest::_confirm(const char* action)
{
    QVERIFY(QMetaObject::invokeMethod(_part("controller"), "confirmAction", Q_ARG(QVariant, _action(action)),
                                      Q_ARG(QVariant, QVariant()), Q_ARG(QVariant, QVariant())));
}

void BoatGuidedActionsTest::_availability_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<bool>("armed");
    QTest::addColumn<bool>("flying");
    QTest::addColumn<bool>("pause");
    QTest::addColumn<bool>("land");
    QTest::addColumn<bool>("altitude");
    QTest::addColumn<bool>("takeoffEntry");
    QTest::newRow("boat-disarmed") << 11 << false << false << false << false << false << false;
    QTest::newRow("boat-armed-not-flying") << 11 << true << false << true << false << false << false;
    QTest::newRow("boat-armed-flying") << 11 << true << true << true << false << false << false;
    QTest::newRow("rover-armed-not-flying") << 10 << true << false << false << true << false << false;
    QTest::newRow("rover-flying") << 10 << true << true << true << true << true << false;
    QTest::newRow("multirotor-ground") << 2 << false << false << false << false << false << true;
    QTest::newRow("multirotor-flying") << 2 << true << true << true << true << true << false;
    QTest::newRow("fixed-wing-flying") << 1 << true << true << true << false << true << true;
    QTest::newRow("vtol-ground") << 20 << false << false << false << false << false << true;
}

void BoatGuidedActionsTest::_availability()
{
    QFETCH(int, kind);
    QFETCH(bool, armed);
    QFETCH(bool, flying);
    QFETCH(bool, pause);
    QFETCH(bool, land);
    QFETCH(bool, altitude);
    QFETCH(bool, takeoffEntry);
    auto* vehicle = _part("vehicleA");
    vehicle->setProperty("kind", kind);
    vehicle->setProperty("armed", armed);
    vehicle->setProperty("flying", flying);
    auto* controller = _part("controller");
    QCOMPARE(controller->property("showPause").toBool(), pause);
    QCOMPARE(controller->property("showLand").toBool(), land);
    QCOMPARE(controller->property("showChangeAlt").toBool(), altitude);
    QCOMPARE(_part("takeoffAction")->property("visible").toBool(), takeoffEntry);
    if (kind == 11) {
        QVERIFY(!controller->property("showTakeoff").toBool());
        QCOMPARE(controller->property("showGotoLocation").toBool(), armed);
        QCOMPARE(controller->property("showRTL").toBool(), armed);
        QVERIFY(!controller->property("startMissionMessage").toString().contains(QStringLiteral("Takeoff")));
    }
}

void BoatGuidedActionsTest::_pauseRouting_data()
{
    QTest::addColumn<int>("kind");
    QTest::newRow("boat-without-altitude") << 11;
    QTest::newRow("rover-existing-altitude-path") << 10;
    QTest::newRow("drone-existing-altitude-path") << 2;
}

void BoatGuidedActionsTest::_pauseRouting()
{
    QFETCH(int, kind);
    auto* vehicle = _part("vehicleA");
    vehicle->setProperty("kind", kind);
    vehicle->setProperty("flying", kind != 11);
    auto* altitude = vehicle->property("altitudeRelative").value<QObject*>();
    QVERIFY(altitude);
    altitude->setProperty("rawValue", kind == 11 ? qQNaN() : 20.0);
    _confirm("actionPause");
    QCOMPARE(_part("slider")->property("visible").toBool(), kind != 11);
    QCOMPARE(_part("slider")->property("setupCount").toInt(), kind == 11 ? 0 : 1);
    QVERIFY(_execute("actionPause"));
    QCOMPARE(vehicle->property("pauseCount").toInt(), kind == 11 ? 1 : 0);
    QCOMPARE(vehicle->property("altitudeCount").toInt(), kind == 11 ? 0 : 1);
    if (kind != 11) {
        QCOMPARE(vehicle->property("lastAltitudeChange").toDouble(), 30.0);
        QVERIFY(vehicle->property("lastPause").toBool());
    }
}

void BoatGuidedActionsTest::_confirmationInvalidation_data()
{
    QTest::addColumn<bool>("disconnect");
    QTest::addColumn<int>("kind");
    for (const int kind : {11, 10, 2}) {
        const QByteArray prefix = QString::number(kind).toLatin1();
        QTest::newRow((prefix + "-vehicle-switch").constData()) << false << kind;
        QTest::newRow((prefix + "-communication-lost-recovered").constData()) << true << kind;
    }
}

void BoatGuidedActionsTest::_confirmationInvalidation()
{
    QFETCH(bool, disconnect);
    QFETCH(int, kind);
    auto* controller = _part("controller");
    auto* vehicle = _part("vehicleA");
    vehicle->setProperty("kind", kind);
    vehicle->setProperty("flying", kind != 11);
    _confirm("actionPause");
    QVERIFY(_part("dialog")->property("visible").toBool());
    if (disconnect) {
        auto* link = vehicle->property("vehicleLinkManager").value<QObject*>();
        QVERIFY(link);
        link->setProperty("communicationLost", true);
        QVERIFY(!_execute("actionPause", vehicle, true));
        link->setProperty("communicationLost", false);
    } else {
        controller->setProperty("_activeVehicle", QVariant::fromValue(_part("vehicleB")));
        QVERIFY(!_execute("actionPause", vehicle, true));
        QCOMPARE(_part("vehicleB")->property("altitudeCount").toInt(), 0);
        controller->setProperty("_activeVehicle", QVariant::fromValue(vehicle));
    }
    QVERIFY(!_part("dialog")->property("visible").toBool());
    QVERIFY(!_part("dialog")->property("actionVehicle").value<QObject*>());
    // A delayed show callback must not resurrect the cancelled confirmation after recovery.
    QVERIFY(QMetaObject::invokeMethod(_part("dialog"), "_reallyShow"));
    QVERIFY(!_part("dialog")->property("visible").toBool());
    QVERIFY(!_execute("actionPause", nullptr, true));
    QCOMPARE(vehicle->property("pauseCount").toInt(), 0);
}

void BoatGuidedActionsTest::_rejectUnavailableActions()
{
    for (const char* action : {"actionTakeoff", "actionLand", "actionChangeAlt", "actionLandAbort", "actionOrbit"}) {
        _confirm(action);
        QVERIFY(!_part("dialog")->property("visible").toBool());
        QVERIFY(!_execute(action));
    }
    auto* controller = _part("controller");
    controller->setProperty("_activeVehicle", QVariant::fromValue<QObject*>(nullptr));
    QVERIFY(!_execute("actionPause"));
    QVERIFY(!_execute("actionGoto"));
}

void BoatGuidedActionsTest::_missionAndGoto()
{
    auto* vehicle = _part("vehicleA");
    auto* controller = _part("controller");
    vehicle->setProperty("armed", false);
    QVERIFY(controller->property("showStartMission").toBool());
    QVERIFY(_execute("actionStartMission"));
    vehicle->setProperty("armed", true);
    QVERIFY(controller->property("showContinueMission").toBool());
    QVERIFY(_execute("actionContinueMission"));
    QVERIFY(_execute("actionGoto"));
    QCOMPARE(_part("slider")->property("setupCount").toInt(), 0);
    QCOMPARE(vehicle->property("missionCount").toInt(), 2);
    QCOMPARE(vehicle->property("gotoCount").toInt(), 1);
    QVERIFY(_execute("actionRTL"));
    QCOMPARE(vehicle->property("rtlCount").toInt(), 1);
}

void BoatGuidedActionsTest::_resumeAfterMission()
{
    auto* vehicle = _part("vehicleA");
    auto* controller = _part("controller");
    QVERIFY(!vehicle->property("flying").toBool());
    vehicle->setProperty("flightMode", QStringLiteral("Mission"));
    vehicle->setProperty("flightMode", QStringLiteral("Hold"));
    vehicle->setProperty("armed", false);
    QVERIFY(controller->property("showResumeMission").toBool());
    QVERIFY(_execute("actionResumeMission"));
    QCOMPARE(_part("mission")->property("resumeCount").toInt(), 1);
    controller->setProperty("_activeVehicle", QVariant::fromValue(_part("vehicleB")));
    controller->setProperty("_activeVehicle", QVariant::fromValue(vehicle));
    QVERIFY(!controller->property("showResumeMission").toBool());
    QVERIFY(!_execute("actionResumeMission"));
}

void BoatGuidedActionsTest::_capabilityAndStateChanges()
{
    auto* vehicle = _part("vehicleA");
    auto* supports = vehicle->property("supports").value<QObject*>();
    QVERIFY(supports);
    vehicle->setProperty("flightMode", QStringLiteral("Hold"));
    QVERIFY(_part("controller")->property("showPause").toBool());
    _confirm("actionPause");
    supports->setProperty("pauseVehicle", false);
    QVERIFY(!_part("dialog")->property("visible").toBool());
    QVERIFY(!_execute("actionPause"));
    supports->setProperty("guidedMode", false);
    QVERIFY(!_execute("actionGoto"));
    QVERIFY(!_execute("actionRTL"));
    supports->setProperty("guidedMode", true);
    vehicle->setProperty("armed", false);
    QVERIFY(!_execute("actionGoto"));
    QVERIFY(!_execute("actionPause"));
}

void BoatGuidedActionsTest::_mixedVehicleMissionMessage()
{
    auto* controller = _part("controller");
    auto* vehicle = _part("vehicleA");
    vehicle->setProperty("kind", 2);
    _part("selection")->setProperty("count", 2);
    QVERIFY(controller->property("mvStartMissionMessage").toString().contains(QStringLiteral("Takeoff")));
    _part("vehicleB")->setProperty("kind", 11);
    QVERIFY(!controller->property("mvStartMissionMessage").toString().contains(QStringLiteral("Takeoff")));
}

void BoatGuidedActionsTest::_confirmationIsConsumed()
{
    _confirm("actionPause");
    auto* button = _part("dialog")->findChild<QObject*>(QStringLiteral("guidedActionConfirmButton"));
    QVERIFY(button);
    QVERIFY(QMetaObject::invokeMethod(button, "activated"));
    QCOMPARE(_part("vehicleA")->property("pauseCount").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(button, "activated"));
    QCOMPARE(_part("vehicleA")->property("pauseCount").toInt(), 1);
}
