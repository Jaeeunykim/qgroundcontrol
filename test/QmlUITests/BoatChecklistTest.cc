#include "BoatChecklistTest.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>
#include <memory>

#include "AppSettings.h"
#include "ColoredSvgImageProvider.h"
#include "MultiVehicleManager.h"
#include "SettingsManager.h"
#include "Vehicle.h"

UT_REGISTER_TEST(BoatChecklistTest, TestLabel::Integration, TestLabel::Vehicle)

void BoatChecklistTest::initTestCase()
{
    UnitTest::initTestCase();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void BoatChecklistTest::_vehicleSwitch_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<QString>("file");
    QTest::newRow("boat") << int(MAV_TYPE_SURFACE_BOAT) << QStringLiteral("RoverChecklist.qml");
    QTest::newRow("rover") << int(MAV_TYPE_GROUND_ROVER) << QStringLiteral("RoverChecklist.qml");
    QTest::newRow("multirotor") << int(MAV_TYPE_QUADROTOR) << QStringLiteral("MultiRotorChecklist.qml");
    QTest::newRow("fixed-wing") << int(MAV_TYPE_FIXED_WING) << QStringLiteral("FixedWingChecklist.qml");
    QTest::newRow("vtol") << int(MAV_TYPE_VTOL_TILTROTOR) << QStringLiteral("VTOLChecklist.qml");
    QTest::newRow("sub") << int(MAV_TYPE_SUBMARINE) << QStringLiteral("SubChecklist.qml");
}

void BoatChecklistTest::_vehicleSwitch()
{
    QFETCH(int, type);
    QFETCH(QString, file);
    ignoreLogMessage("qt.qpa.fonts", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Populating font family aliases")));
    MultiVehicleManager::instance()->init();
    Vehicle boat(MAV_AUTOPILOT_PX4, MAV_TYPE_SURFACE_BOAT);
    Vehicle next(MAV_AUTOPILOT_PX4, static_cast<MAV_TYPE>(type));
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/unittest/BoatChecklistHarness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(
        component.createWithInitialProperties({{QStringLiteral("activeVehicle"), QVariant::fromValue(&boat)}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    auto* checklist = root->property("checklist").value<QObject*>();
    QVERIFY(checklist);
    auto* loader = checklist->findChild<QObject*>(QStringLiteral("preFlightChecklist_model"));
    auto* delay = checklist->findChild<QObject*>(QStringLiteral("preFlightChecklist_delay"));
    QVERIFY(loader && delay);
    QTRY_VERIFY_WITH_TIMEOUT(
        loader->property("source").toUrl().toString().endsWith(QStringLiteral("RoverChecklist.qml")),
        TestTimeout::mediumMs());
    auto* title =
        loader->property("item").value<QObject*>()->findChild<QObject*>(QStringLiteral("roverChecklist_initialChecks"));
    auto* mission =
        loader->property("item").value<QObject*>()->findChild<QObject*>(QStringLiteral("roverChecklist_missionCheck"));
    QVERIFY(title && mission);
    QCOMPARE(title->property("name").toString(), QStringLiteral("Boat Initial Checks"));
    QVERIFY(mission->property("manualText").toString().contains(QStringLiteral("water route")));
    checklist->setProperty("allChecksPassed", true);
    QCOMPARE(boat.checkListState(), Vehicle::CheckListPassed);
    QVERIFY(QMetaObject::invokeMethod(checklist, "groupPassedChanged", Q_ARG(QVariant, 0), Q_ARG(QVariant, true)));
    QVERIFY(delay->property("running").toBool());
    root->setProperty("activeVehicle", QVariant::fromValue(&next));
    QTRY_VERIFY_WITH_TIMEOUT(loader->property("source").toUrl().toString().endsWith(file), TestTimeout::mediumMs());
    QVERIFY(!checklist->property("allChecksPassed").toBool());
    QVERIFY(!delay->property("running").toBool());
    QCOMPARE(next.checkListState(), Vehicle::CheckListFailed);
    QCOMPARE(boat.checkListState(), Vehicle::CheckListPassed);
    if (type == MAV_TYPE_SURFACE_BOAT || type == MAV_TYPE_GROUND_ROVER) {
        title = loader->property("item").value<QObject*>()->findChild<QObject*>(
            QStringLiteral("roverChecklist_initialChecks"));
        QVERIFY(title);
        QCOMPARE(title->property("name").toString(), type == MAV_TYPE_SURFACE_BOAT
                                                         ? QStringLiteral("Boat Initial Checks")
                                                         : QStringLiteral("Rover Initial Checks"));
    }
    root->setProperty("activeVehicle", QVariant::fromValue<Vehicle*>(nullptr));
    QVERIFY(!checklist->property("allChecksPassed").toBool());
}

void BoatChecklistTest::_sameVehicleTypeChange()
{
    ignoreLogMessage("qt.qpa.fonts", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Populating font family aliases")));
    MultiVehicleManager::instance()->init();
    auto* type = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    auto* vehicle = MultiVehicleManager::instance()->offlineEditingVehicle();
    QVERIFY(vehicle);
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/unittest/BoatChecklistHarness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(
        component.createWithInitialProperties({{QStringLiteral("activeVehicle"), QVariant::fromValue(vehicle)}}));
    QVERIFY2(root, qPrintable(component.errorString()));
    auto* checklist = root->property("checklist").value<QObject*>();
    QVERIFY(checklist);
    auto* loader = checklist->findChild<QObject*>(QStringLiteral("preFlightChecklist_model"));
    QVERIFY(loader);
    checklist->setProperty("allChecksPassed", true);
    type->setRawValue(MAV_TYPE_QUADROTOR);
    QTRY_VERIFY_WITH_TIMEOUT(
        loader->property("source").toUrl().toString().endsWith(QStringLiteral("MultiRotorChecklist.qml")),
        TestTimeout::mediumMs());
    QVERIFY(!checklist->property("allChecksPassed").toBool());
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    QTRY_VERIFY_WITH_TIMEOUT(
        loader->property("source").toUrl().toString().endsWith(QStringLiteral("RoverChecklist.qml")),
        TestTimeout::mediumMs());
    auto* title =
        loader->property("item").value<QObject*>()->findChild<QObject*>(QStringLiteral("roverChecklist_initialChecks"));
    QVERIFY(title);
    QCOMPARE(title->property("name").toString(), QStringLiteral("Rover Initial Checks"));
}
