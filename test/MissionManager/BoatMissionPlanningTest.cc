#include "BoatMissionPlanningTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>

#include "AppSettings.h"
#include "MissionCommandTree.h"
#include "MissionCommandUIInfo.h"
#include "MissionController.h"
#include "MissionSettingsItem.h"
#include "PlanMasterController.h"
#include "SettingsManager.h"
#include "SimpleMissionItem.h"
#include "SpeedSection.h"
#include "Vehicle.h"

UT_REGISTER_TEST(BoatMissionPlanningTest, TestLabel::Unit, TestLabel::MissionManager)

void BoatMissionPlanningTest::init()
{
    setOfflineVehicleType(MAV_TYPE_SURFACE_BOAT);
    setOfflineFirmwareType(MAV_AUTOPILOT_PX4);
    if (qstrcmp(QTest::currentTestFunction(), "_speedRoundTrip") == 0) {
        QFETCH(int, firmware);
        QFETCH(int, type);
        setOfflineFirmwareType(static_cast<MAV_AUTOPILOT>(firmware));
        setOfflineVehicleType(static_cast<MAV_TYPE>(type));
    }
    OfflineMissionTest::init();
}

void BoatMissionPlanningTest::_speedRoundTrip_data()
{
    QTest::addColumn<int>("firmware");
    QTest::addColumn<int>("type");
    QTest::addColumn<double>("speed");
    for (const int firmware : {int(MAV_AUTOPILOT_PX4), int(MAV_AUTOPILOT_ARDUPILOTMEGA)}) {
        for (const int type : {int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_QUADROTOR),
                               int(MAV_TYPE_FIXED_WING), int(MAV_TYPE_VTOL_TAILSITTER_QUADROTOR)}) {
            for (const double speed : {0.0, 2.5, 30.0}) {
                const QByteArray name =
                    QStringLiteral("fw%1-type%2-speed%3").arg(firmware).arg(type).arg(speed).toLatin1();
                QTest::newRow(name.constData()) << firmware << type << speed;
            }
        }
    }
}

void BoatMissionPlanningTest::_speedRoundTrip()
{
    QFETCH(int, firmware);
    SettingsManager::instance()->appSettings()->offlineEditingFirmwareClass()->setRawValue(
        QGCMAVLink::firmwareClass(static_cast<MAV_AUTOPILOT>(firmware)));
    QFETCH(int, type);
    QFETCH(double, speed);
    SettingsManager::instance()->appSettings()->offlineEditingVehicleClass()->setRawValue(type);
    QCOMPARE(int(planController()->controllerVehicle()->vehicleType()), type);
    auto* mission = missionController();
    mission->insertSimpleMissionItem(QGeoCoordinate(47.0, 8.0, 0), 1, false);
    auto* waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(waypoint);
    const bool available = type == MAV_TYPE_SURFACE_BOAT || type == MAV_TYPE_QUADROTOR || type == MAV_TYPE_FIXED_WING;
    QCOMPARE(waypoint->speedSection()->available(), available);
    auto* settings = mission->visualItems()->value<MissionSettingsItem*>(0);
    QVERIFY(settings);
    QCOMPARE(settings->speedSection()->available(), available);
    if (!available) {
        return;  // Rover/VTOL availability remains unchanged.
    }
    settings->speedSection()->setSpecifyFlightSpeed(true);
    settings->speedSection()->flightSpeed()->setRawValue(speed);
    waypoint->speedSection()->setSpecifyFlightSpeed(true);
    waypoint->speedSection()->flightSpeed()->setRawValue(speed);
    QSignalSpy availableChanged(waypoint->speedSection(), &SpeedSection::availableChanged);
    waypoint->speedSection()->setAvailable(false);
    QVERIFY(!waypoint->speedSection()->available());
    QCOMPARE(availableChanged.count(), 1);
    waypoint->speedSection()->setAvailable(true);
    QVERIFY(waypoint->speedSection()->available());
    const int speedType = type == MAV_TYPE_FIXED_WING ? 0 : 1;
    const QJsonObject originalMission = planController()->saveToJson().object()[QStringLiteral("mission")].toObject();
    const QJsonArray items = originalMission[QStringLiteral("items")].toArray();
    int speedCount = 0;
    for (const QJsonValue& value : items) {
        const auto item = value.toObject();
        if (item[QStringLiteral("command")].toInt() == MAV_CMD_DO_CHANGE_SPEED) {
            const auto params = item[QStringLiteral("params")].toArray();
            QCOMPARE(params[0].toInt(), speedType);
            QCOMPARE(params[1].toDouble(), speed);
            QCOMPARE(params[2].toDouble(), -1.0);
            speedCount++;
        }
    }
    QCOMPARE(speedCount, 2);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("speed.plan"));
    QVERIFY(planController()->saveToFile(file));
    planController()->loadFromFile(file);
    QCOMPARE(mission->visualItems()->count(), 2);
    settings = mission->visualItems()->value<MissionSettingsItem*>(0);
    waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(settings && waypoint);
    QVERIFY(settings->speedSection()->specifyFlightSpeed());
    QVERIFY(waypoint->speedSection()->specifyFlightSpeed());
    QCOMPARE(settings->speedSection()->specifiedFlightSpeed(), speed);
    QCOMPARE(waypoint->speedSection()->specifiedFlightSpeed(), speed);
    QCOMPARE(planController()
                 ->saveToJson()
                 .object()[QStringLiteral("mission")]
                 .toObject()[QStringLiteral("items")]
                 .toArray(),
             items);
}

void BoatMissionPlanningTest::_unmergedSpeedCommands_data()
{
    QTest::addColumn<int>("speedType");
    QTest::addColumn<double>("speed");
    QTest::newRow("airspeed") << 0 << 3.0;
    QTest::newRow("climb-speed") << 2 << 3.0;
    QTest::newRow("no-change") << 1 << -1.0;
    QTest::newRow("default-speed") << 1 << -2.0;
    QTest::newRow("missing-speed") << 1 << qQNaN();
}

void BoatMissionPlanningTest::_unmergedSpeedCommands()
{
    QFETCH(int, speedType);
    QFETCH(double, speed);
    SettingsManager::instance()->appSettings()->offlineEditingVehicleClass()->setRawValue(MAV_TYPE_SURFACE_BOAT);
    SpeedSection section(planController());
    section.setAvailable(true);
    MissionItem command(1, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION, speedType, speed, -1, 0, 0, 0, 0, true, false);
    auto* item = new SimpleMissionItem(planController(), false, command);
    QmlObjectListModel items;
    items.append(item);
    QVERIFY(!section.scanForSection(&items, 0));
    QCOMPARE(items.count(), 1);
    QVERIFY(!section.specifyFlightSpeed());
    QCOMPARE(item->missionItem().param1(), double(speedType));
    if (qIsNaN(speed)) {
        QVERIFY(qIsNaN(item->missionItem().param2()));
    } else {
        QCOMPARE(item->missionItem().param2(), speed);
    }
}

void BoatMissionPlanningTest::_altitudePreservation_data()
{
    QTest::addColumn<int>("frame");
    QTest::newRow("relative") << int(MAV_FRAME_GLOBAL_RELATIVE_ALT);
    QTest::newRow("absolute") << int(MAV_FRAME_GLOBAL);
    QTest::newRow("terrain") << int(MAV_FRAME_GLOBAL_TERRAIN_ALT);
}

void BoatMissionPlanningTest::_altitudePreservation()
{
    QFETCH(int, frame);
    SettingsManager::instance()->appSettings()->offlineEditingVehicleClass()->setRawValue(MAV_TYPE_SURFACE_BOAT);
    auto* mission = missionController();
    mission->setGlobalAltitudeFrame(QGroundControlQmlGlobal::AltitudeFrameCalcAboveTerrain);
    mission->insertSimpleMissionItem(QGeoCoordinate(47, 8, 0), 1, false);
    auto* waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(waypoint);
    QCOMPARE(waypoint->altitudeFrame(), QGroundControlQmlGlobal::AltitudeFrameRelative);
    QCOMPARE(waypoint->readyForSaveState(), VisualMissionItem::ReadyForSave);
    const double altitude = 123.5;
    waypoint->setAltitudeFrame(frame == MAV_FRAME_GLOBAL ? QGroundControlQmlGlobal::AltitudeFrameAbsolute
                               : frame == MAV_FRAME_GLOBAL_TERRAIN_ALT
                                   ? QGroundControlQmlGlobal::AltitudeFrameTerrain
                                   : QGroundControlQmlGlobal::AltitudeFrameRelative);
    waypoint->altitude()->setRawValue(altitude);
    QCOMPARE(waypoint->missionItem().param7(), altitude);
    QCOMPARE(waypoint->missionItem().frame(), MAV_FRAME(frame));
    QVERIFY(qIsNaN(waypoint->terrainAltitude()));
    QCOMPARE(waypoint->readyForSaveState(), VisualMissionItem::ReadyForSave);
    const auto before = planController()
                            ->saveToJson()
                            .object()[QStringLiteral("mission")]
                            .toObject()[QStringLiteral("items")]
                            .toArray();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto file = dir.filePath(QStringLiteral("altitude.plan"));
    QVERIFY(planController()->saveToFile(file));
    planController()->loadFromFile(file);
    waypoint = mission->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(waypoint);
    QCOMPARE(waypoint->missionItem().frame(), MAV_FRAME(frame));
    QCOMPARE(waypoint->missionItem().param7(), altitude);
    QCOMPARE(planController()
                 ->saveToJson()
                 .object()[QStringLiteral("mission")]
                 .toObject()[QStringLiteral("items")]
                 .toArray(),
             before);
}

void BoatMissionPlanningTest::_commandFilteringDoesNotLeak()
{
    auto* vehicle = planController()->controllerVehicle();
    auto* type = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    auto* tree = MissionCommandTree::instance();
    for (const int kind : {int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_QUADROTOR),
                           int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER)}) {
        type->setRawValue(kind);
        const auto commands = tree->getCommandsForCategory(vehicle, QStringLiteral("All commands"), true);
        bool waypoint = false;
        bool altitudeLoiter = false;
        for (const auto& command : commands) {
            auto* info = command.value<MissionCommandUIInfo*>();
            QVERIFY(info);
            waypoint |= info->command() == MAV_CMD_NAV_WAYPOINT;
            altitudeLoiter |= info->command() == MAV_CMD_NAV_LOITER_TO_ALT;
            if (kind == MAV_TYPE_SURFACE_BOAT) {
                QVERIFY(!tree->isAirborneOnlyCommand(info->command()));
            }
        }
        QVERIFY(waypoint);
        QCOMPARE(altitudeLoiter, kind != MAV_TYPE_SURFACE_BOAT);
        // Metadata must remain available for imported aircraft commands and shared Rover cache entries.
        QVERIFY(tree->getUIInfo(vehicle, QGCMAVLink::VehicleClassGeneric, MAV_CMD_NAV_TAKEOFF));
    }
}

void BoatMissionPlanningTest::_importCalculatedAltitude_data()
{
    QTest::addColumn<double>("wireAltitude");
    QTest::newRow("resolved-wire-altitude") << 123.5;
    QTest::newRow("unresolved-wire-altitude") << qQNaN();
}

void BoatMissionPlanningTest::_importCalculatedAltitude()
{
    QFETCH(double, wireAltitude);
    SettingsManager::instance()->appSettings()->offlineEditingVehicleClass()->setRawValue(MAV_TYPE_SURFACE_BOAT);
    MissionItem command(1, MAV_CMD_NAV_WAYPOINT, MAV_FRAME_GLOBAL, 0, 0, 0, qQNaN(), 47, 8, wireAltitude, true, false);
    QJsonObject original;
    command.save(original);
    original[QStringLiteral("AltitudeMode")] = QGroundControlQmlGlobal::AltitudeFrameCalcAboveTerrain;
    original[QStringLiteral("Altitude")] = 50.0;
    original[QStringLiteral("AMSLAltAboveTerrain")] = QJsonValue::Null;
    SimpleMissionItem imported(planController(), false, true);
    QString error;
    QVERIFY2(imported.load(original, 1, error), qPrintable(error));
    QVERIFY(QMetaObject::invokeMethod(&imported, "_terrainAltChanged"));
    QCOMPARE(imported.missionItem().frame(), MAV_FRAME_GLOBAL);
    if (qIsNaN(wireAltitude)) {
        QVERIFY(qIsNaN(imported.missionItem().param7()));
        QCOMPARE(imported.readyForSaveState(), VisualMissionItem::NotReadyForSaveData);
    } else {
        QCOMPARE(imported.missionItem().param7(), wireAltitude);
        QCOMPARE(imported.readyForSaveState(), VisualMissionItem::ReadyForSave);
    }
    QJsonArray saved;
    imported.save(saved);
    QCOMPARE(saved.count(), 1);
    QCOMPARE(QJsonDocument(saved[0].toObject()).toJson(QJsonDocument::Compact),
             QJsonDocument(original).toJson(QJsonDocument::Compact));
}

void BoatMissionPlanningTest::_availabilityFollowsOfflineType()
{
    auto* settings = missionController()->visualItems()->value<MissionSettingsItem*>(0);
    QVERIFY(settings);
    auto* speed = settings->speedSection();
    QSignalSpy changes(speed, &SpeedSection::availableChanged);
    auto* type = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    for (const int kind :
         {int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_QUADROTOR), int(MAV_TYPE_FIXED_WING),
          int(MAV_TYPE_VTOL_TAILSITTER_QUADROTOR), int(MAV_TYPE_SURFACE_BOAT)}) {
        type->setRawValue(kind);
        QCOMPARE(int(planController()->controllerVehicle()->vehicleType()), kind);
        QCOMPARE(speed->available(),
                 kind == MAV_TYPE_SURFACE_BOAT || kind == MAV_TYPE_QUADROTOR || kind == MAV_TYPE_FIXED_WING);
        QVERIFY(!speed->specifyFlightSpeed());
        QCOMPARE(speed->itemCount(), 0);
        QCOMPARE(missionController()->visualItems()->count(), 1);
    }
    QCOMPARE(changes.count(), 4);
    speed->setAvailable(false);
    type->setRawValue(MAV_TYPE_QUADROTOR);
    QVERIFY(!speed->available());  // A caller-disabled Section must stay disabled across vehicle changes.
}
