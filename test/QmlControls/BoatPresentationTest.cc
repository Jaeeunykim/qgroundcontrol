#include "BoatPresentationTest.h"

#include <QtCore/QMetaProperty>
#include <QtCore/QSettings>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtSvg/QSvgRenderer>
#include <QtTest/QSignalSpy>

#include "AppSettings.h"
#include "FactValueGrid.h"
#include "FirmwarePlugin.h"
#include "InstrumentValueData.h"
#include "MultiVehicleManager.h"
#include "PlanMasterController.h"
#include "SettingsManager.h"
#include "Vehicle.h"

UT_REGISTER_TEST(BoatPresentationTest, TestLabel::Unit, TestLabel::Vehicle)

void BoatPresentationTest::init()
{
    setOfflineVehicleType(MAV_TYPE_SURFACE_BOAT);
    if (qstrcmp(QTest::currentTestFunction(), "_defaults") == 0 ||
        qstrcmp(QTest::currentTestFunction(), "_vehicleIcons") == 0) {
        QFETCH(int, type);
        setOfflineVehicleType(static_cast<MAV_TYPE>(type));
    }
    OfflineMissionTest::init();
    MultiVehicleManager::instance()->init();
}

void BoatPresentationTest::_completeGrid(FactValueGrid& grid, const QString& group, bool card, Vehicle* cardVehicle)
{
    QVERIFY(grid.setProperty("settingsGroup", group));
    if (card && !cardVehicle) {
        cardVehicle = planController()->controllerVehicle();
    }
    QVERIFY(grid.setProperty("specificVehicleForCard", QVariant::fromValue(card ? cardVehicle : nullptr)));
    grid.componentComplete();
    QVERIFY(grid.currentVehicle());
    QVERIFY(grid.columns());
}

InstrumentValueData* BoatPresentationTest::_value(FactValueGrid& grid, int column, int row)
{
    return grid.columns()->value<QmlObjectListModel*>(column)->value<InstrumentValueData*>(row);
}

void BoatPresentationTest::_defaults_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("card");
    for (const int type : {int(MAV_TYPE_SURFACE_BOAT), int(MAV_TYPE_GROUND_ROVER), int(MAV_TYPE_QUADROTOR),
                           int(MAV_TYPE_FIXED_WING), int(MAV_TYPE_VTOL_TAILSITTER_QUADROTOR)}) {
        for (const bool card : {false, true}) {
            QTest::newRow(QStringLiteral("type%1-card%2").arg(type).arg(card).toLatin1().constData()) << type << card;
        }
    }
}

void BoatPresentationTest::_defaults()
{
    QFETCH(int, type);
    QFETCH(bool, card);
    FactValueGrid grid;
    _completeGrid(grid, QStringLiteral("BoatPresentationDefaults-%1-%2").arg(type).arg(card), card);
    if (QTest::currentTestFailed()) {
        return;
    }
    if (type == MAV_TYPE_SURFACE_BOAT) {
        QCOMPARE(grid.columns()->count(), 2);
        QCOMPARE(grid.property("rowCount").toInt(), card ? 1 : 2);
        QCOMPARE(_value(grid)->factName(), QStringLiteral("GroundSpeed"));
        QCOMPARE(_value(grid, card ? 1 : 0, card ? 0 : 1)->factName(), QStringLiteral("Heading"));
        if (!card) {
            QCOMPARE(_value(grid, 1, 0)->factName(), QStringLiteral("DistanceToHome"));
            QCOMPARE(_value(grid, 1, 1)->factName(), QStringLiteral("DistanceToNextWP"));
        }
        for (int column = 0; column < grid.columns()->count(); column++) {
            for (int row = 0; row < grid.property("rowCount").toInt(); row++) {
                auto* value = _value(grid, column, row);
                QVERIFY(value->fact());
                QVERIFY(value->factName() != QStringLiteral("AltitudeRelative"));
                QVERIFY(value->factName() != QStringLiteral("ClimbRate"));
            }
        }
        for (auto* fact : {_value(grid)->fact(), _value(grid, card ? 1 : 0, card ? 0 : 1)->fact()}) {
            fact->setRawValue(qQNaN());
            QVERIFY(qIsNaN(fact->rawValue().toDouble()));
            const QString missing = fact->cookedValueString();
            fact->setRawValue(0.0);
            QCOMPARE(fact->rawValue().toDouble(), 0.0);
            QVERIFY(fact->cookedValueString() != missing);
        }
    } else {
        QCOMPARE(_value(grid)->factName(), QStringLiteral("AltitudeRelative"));
        const bool fixedWingValues = type == MAV_TYPE_FIXED_WING || type == MAV_TYPE_VTOL_TAILSITTER_QUADROTOR;
        if (card) {
            QCOMPARE(grid.columns()->count(), 2);
            QCOMPARE(grid.property("rowCount").toInt(), 1);
            QCOMPARE(_value(grid, 1)->factName(),
                     fixedWingValues ? QStringLiteral("AirSpeed") : QStringLiteral("GroundSpeed"));
        } else {
            QCOMPARE(grid.columns()->count(), fixedWingValues ? 4 : 3);
            QCOMPARE(_value(grid, 1)->factName(), QStringLiteral("ClimbRate"));
        }
    }
}

void BoatPresentationTest::_vehicleIcons_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("boat") << int(MAV_TYPE_SURFACE_BOAT);
    QTest::newRow("rover") << int(MAV_TYPE_GROUND_ROVER);
    QTest::newRow("drone") << int(MAV_TYPE_QUADROTOR);
}

void BoatPresentationTest::_vehicleIcons()
{
    QFETCH(int, type);
    auto* vehicle = planController()->controllerVehicle();
    QVERIFY(vehicle);
    for (const bool outline : {false, true}) {
        const QString image = outline ? vehicle->vehicleImageOutline() : vehicle->vehicleImageOpaque();
        QCOMPARE(image, type == MAV_TYPE_SURFACE_BOAT
                            ? (outline ? QStringLiteral("/qmlimages/boatOutline.svg")
                                       : QStringLiteral("/qmlimages/boatOpaque.svg"))
                            : (outline ? QStringLiteral("/qmlimages/vehicleArrowOutline.svg")
                                       : QStringLiteral("/qmlimages/vehicleArrowOpaque.svg")));
        QSvgRenderer renderer(QStringLiteral(":") + image);
        QVERIFY(renderer.isValid());
        QImage rendered(72, 72, QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        renderer.render(&painter);
        painter.end();
        QVERIFY(!rendered.isNull());
        QVERIFY(qAlpha(rendered.pixel(36, 5)) > 0);
    }
    for (const char* name : {"vehicleImageOpaque", "vehicleImageOutline"}) {
        const auto property = vehicle->metaObject()->property(vehicle->metaObject()->indexOfProperty(name));
        QVERIFY(!property.isConstant());
        QVERIFY(property.hasNotifySignal());
        QCOMPARE(property.notifySignal().name(), QByteArray("vehicleTypeChanged"));
    }
    auto* typeFact = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("testVehicle"), vehicle);
    QQmlComponent component(&engine);
    component.setData("import QtQml; QtObject { property string icon: testVehicle.vehicleImageOpaque }", QUrl());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> boundIcon(component.create());
    QVERIFY(boundIcon);
    QCOMPARE(boundIcon->property("icon").toString(), vehicle->vehicleImageOpaque());
    QSignalSpy changed(vehicle, &Vehicle::vehicleTypeChanged);
    typeFact->setRawValue(type == MAV_TYPE_SURFACE_BOAT ? MAV_TYPE_GROUND_ROVER : MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(vehicle->vehicleImageOpaque().contains(QStringLiteral("boatOpaque")), type != MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(boundIcon->property("icon").toString(), vehicle->vehicleImageOpaque());
    FirmwarePlugin defaultPlugin;
    QCOMPARE(defaultPlugin.vehicleImageOpaque(nullptr), QStringLiteral("/qmlimages/vehicleArrowOpaque.svg"));
}

void BoatPresentationTest::_legacyLayoutMigration()
{
    auto* type = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    const QString group = QStringLiteral("BoatPresentationMigration");
    const QString legacyKey = QStringLiteral("%1-%2").arg(group).arg(QGCMAVLink::VehicleClassRoverBoat);
    FactValueGrid grid;
    _completeGrid(grid, group, true);
    if (QTest::currentTestFailed()) {
        return;
    }
    _value(grid)->setFact(QStringLiteral("Vehicle"), QStringLiteral("Heading"));
    _value(grid)->setText(QStringLiteral("Saved helm"));
    grid.setFontSize(FactValueGrid::LargeFontSize);
    QSettings settings;
    settings.sync();
    const auto readGroup = [&settings](const QString& name) {
        QVariantMap values;
        settings.beginGroup(name);
        const auto keys = settings.allKeys();
        for (const auto& key : keys) {
            values.insert(key, settings.value(key));
        }
        settings.endGroup();
        return values;
    };
    const auto original = readGroup(legacyKey);
    QVERIFY(!original.isEmpty());
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(_value(grid)->text(), QStringLiteral("Saved helm"));
    QCOMPARE(_value(grid)->factName(), QStringLiteral("Heading"));
    QCOMPARE(grid.fontSize(), FactValueGrid::LargeFontSize);
    QCOMPARE(readGroup(legacyKey), original);
    QCOMPARE(readGroup(group + QStringLiteral("-Boat")), original);
    _value(grid)->setText(QStringLiteral("Boat helm"));
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    QCOMPARE(_value(grid)->text(), QStringLiteral("Saved helm"));
    QCOMPARE(readGroup(legacyKey), original);
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(_value(grid)->text(), QStringLiteral("Boat helm"));
    {
        FactValueGrid reloaded;
        _completeGrid(reloaded, group, true);
        QCOMPARE(_value(reloaded)->text(), QStringLiteral("Boat helm"));
    }
    grid.resetToDefaults();
    QCOMPARE(_value(grid)->factName(), QStringLiteral("GroundSpeed"));
    QCOMPARE(readGroup(legacyKey), original);
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    QCOMPARE(_value(grid)->text(), QStringLiteral("Saved helm"));
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(_value(grid)->factName(), QStringLiteral("GroundSpeed"));
    QVERIFY(settings.value(group + QStringLiteral("-BoatInitialized")).toBool());
}

void BoatPresentationTest::_existingBoatLayoutWins()
{
    const QString group = QStringLiteral("BoatPresentationExisting");
    FactValueGrid grid;
    _completeGrid(grid, group, true);
    if (QTest::currentTestFailed()) {
        return;
    }
    _value(grid)->setText(QStringLiteral("Boat saved"));
    QSettings settings;
    settings.remove(group + QStringLiteral("-BoatInitialized"));
    auto* type = SettingsManager::instance()->appSettings()->offlineEditingVehicleClass();
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    _value(grid)->setText(QStringLiteral("Rover saved"));
    type->setRawValue(MAV_TYPE_SURFACE_BOAT);
    QCOMPARE(_value(grid)->text(), QStringLiteral("Boat saved"));
    type->setRawValue(MAV_TYPE_QUADROTOR);
    QCOMPARE(_value(grid)->factName(), QStringLiteral("AltitudeRelative"));
    type->setRawValue(MAV_TYPE_GROUND_ROVER);
    QCOMPARE(_value(grid)->text(), QStringLiteral("Rover saved"));
}

void BoatPresentationTest::_mixedVehicleCards()
{
    Vehicle boat(MAV_AUTOPILOT_PX4, MAV_TYPE_SURFACE_BOAT);
    Vehicle rover(MAV_AUTOPILOT_PX4, MAV_TYPE_GROUND_ROVER);
    Vehicle drone(MAV_AUTOPILOT_PX4, MAV_TYPE_QUADROTOR);
    const QString group = QStringLiteral("BoatPresentationMixedCards");
    FactValueGrid boatGrid;
    FactValueGrid roverGrid;
    FactValueGrid droneGrid;
    _completeGrid(boatGrid, group, true, &boat);
    _completeGrid(roverGrid, group, true, &rover);
    _completeGrid(droneGrid, group, true, &drone);
    if (QTest::currentTestFailed()) {
        return;
    }
    _value(roverGrid)->setText(QStringLiteral("Rover custom"));
    QCOMPARE(_value(boatGrid)->factName(), QStringLiteral("GroundSpeed"));
    _value(boatGrid)->setText(QStringLiteral("Boat custom"));
    _value(droneGrid)->setText(QStringLiteral("Drone custom"));
    QCOMPARE(_value(roverGrid)->text(), QStringLiteral("Rover custom"));
    QCOMPARE(_value(boatGrid)->text(), QStringLiteral("Boat custom"));
    QCOMPARE(_value(droneGrid)->text(), QStringLiteral("Drone custom"));
    QCOMPARE(_value(roverGrid)->fact(), rover.altitudeRelative());
    QCOMPARE(_value(boatGrid)->fact(), boat.groundSpeed());
    QCOMPARE(_value(droneGrid)->fact(), drone.altitudeRelative());
    boatGrid.resetToDefaults();
    QCOMPARE(_value(boatGrid)->factName(), QStringLiteral("GroundSpeed"));
    QCOMPARE(_value(roverGrid)->text(), QStringLiteral("Rover custom"));
    QCOMPARE(_value(droneGrid)->text(), QStringLiteral("Drone custom"));
}
