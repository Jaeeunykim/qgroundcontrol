#include "BoatPlanEditorsTest.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuickControls2/QQuickStyle>
#include <memory>

#include "AppSettings.h"
#include "ColoredSvgImageProvider.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "QGCPalette.h"
#include "SettingsManager.h"
#include "SimpleMissionItem.h"

UT_REGISTER_TEST(BoatPlanEditorsTest, TestLabel::Integration, TestLabel::MissionManager)

void BoatPlanEditorsTest::initTestCase()
{
    UnitTest::initTestCase();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void BoatPlanEditorsTest::init()
{
    QFETCH(int, type);
    setOfflineVehicleType(static_cast<MAV_TYPE>(type));
    OfflineMissionTest::init();
}

void BoatPlanEditorsTest::_editors_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("boat") << int(MAV_TYPE_SURFACE_BOAT);
    QTest::newRow("rover") << int(MAV_TYPE_GROUND_ROVER);
    QTest::newRow("multirotor") << int(MAV_TYPE_QUADROTOR);
    QTest::newRow("fixed-wing") << int(MAV_TYPE_FIXED_WING);
    QTest::newRow("vtol") << int(MAV_TYPE_VTOL_TAILSITTER_QUADROTOR);
}

void BoatPlanEditorsTest::_editors()
{
    QFETCH(int, type);
    ignoreLogMessage("qt.qpa.fonts", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Populating font family aliases")));
    SettingsManager::instance()->appSettings()->offlineEditingVehicleClass()->setRawValue(type);
    missionController()->insertSimpleMissionItem(QGeoCoordinate(47, 8, 0), 1, false);
    auto* item = missionController()->visualItems()->value<SimpleMissionItem*>(1);
    QVERIFY(item);
    QGCPalette palette;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("qgcPal"), &palette);
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    const bool boat = type == MAV_TYPE_SURFACE_BOAT;
    const bool speedAvailable = boat || type == MAV_TYPE_QUADROTOR || type == MAV_TYPE_FIXED_WING;
    const QVariantMap properties{{QStringLiteral("missionController"), QVariant::fromValue(missionController())},
                                 {QStringLiteral("planMasterController"), QVariant::fromValue(planController())}};
    QQmlComponent defaultsComponent(&engine,
                                    QUrl(QStringLiteral("qrc:/qml/QGroundControl/PlanView/MissionDefaultsEditor.qml")));
    QVERIFY2(defaultsComponent.isReady(), qPrintable(defaultsComponent.errorString()));
    std::unique_ptr<QObject> defaults(defaultsComponent.createWithInitialProperties(properties));
    QVERIFY2(defaults, qPrintable(defaultsComponent.errorString()));
    for (const auto* name : {"missionDefaults_altFrame", "missionDefaults_altitude"}) {
        auto* control = defaults->findChild<QQuickItem*>(QLatin1String(name));
        QVERIFY(control);
        QCOMPARE(control->isVisible(), !boat);
    }
    auto* speed = defaults->findChild<QQuickItem*>(QStringLiteral("missionDefaults_speed"));
    QVERIFY(speed);
    QCOMPARE(speed->isVisible(), speedAvailable);
    QCOMPARE(speed->property("label").toString(),
             boat ? QStringLiteral("Ground Speed") : QStringLiteral("Flight Speed"));
    QQmlComponent itemComponent(&engine, QUrl(QStringLiteral("qrc:/qml/QGroundControl/PlanView/SimpleItemEditor.qml")));
    QVERIFY2(itemComponent.isReady(), qPrintable(itemComponent.errorString()));
    std::unique_ptr<QObject> editor(itemComponent.createWithInitialProperties(
        {{QStringLiteral("missionItem"), QVariant::fromValue(item)}, {QStringLiteral("availableWidth"), 400}}));
    QVERIFY2(editor, qPrintable(itemComponent.errorString()));
    auto* altitude = editor->findChild<QQuickItem*>(QStringLiteral("simpleItem_altitude"));
    QVERIFY(altitude);
    QCOMPARE(altitude->isVisible(), !boat);
    QQmlComponent statsComponent(&engine, QUrl(QStringLiteral("qrc:/qml/QGroundControl/PlanView/MissionStats.qml")));
    QVERIFY2(statsComponent.isReady(), qPrintable(statsComponent.errorString()));
    std::unique_ptr<QObject> stats(statsComponent.createWithInitialProperties(
        {{QStringLiteral("planMasterController"), QVariant::fromValue(planController())}}));
    QVERIFY2(stats, qPrintable(statsComponent.errorString()));
    stats->setProperty("_missionTime", qQNaN());
    QVariant timeText;
    QVERIFY(QMetaObject::invokeMethod(stats.get(), "getMissionTime", Q_RETURN_ARG(QVariant, timeText)));
    QCOMPARE(timeText.toString(), boat ? QStringLiteral("N/A") : QStringLiteral("00:00:00"));
    for (const auto* name : {"missionStats_altDifference", "missionStats_gradient"}) {
        auto* value = stats->findChild<QQuickItem*>(QLatin1String(name));
        QVERIFY(value);
        QCOMPARE(value->isVisible(), !boat);
    }
}
