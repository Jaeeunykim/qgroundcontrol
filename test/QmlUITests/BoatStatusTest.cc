#include "BoatStatusTest.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>
#include <memory>

#include "ColoredSvgImageProvider.h"

UT_REGISTER_TEST(BoatStatusTest, TestLabel::Integration, TestLabel::Vehicle)

void BoatStatusTest::initTestCase()
{
    UnitTest::initTestCase();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void BoatStatusTest::_status_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("armed");
    QTest::addColumn<bool>("flying");
    QTest::addColumn<bool>("landing");
    QTest::addColumn<QString>("text");
    QTest::newRow("boat-ready") << 11 << false << false << false << QStringLiteral("Ready");
    QTest::newRow("boat-armed") << 11 << true << false << false << QStringLiteral("Armed");
    QTest::newRow("boat-flying-flag") << 11 << true << true << false << QStringLiteral("Armed");
    QTest::newRow("boat-landing-flag") << 11 << true << false << true << QStringLiteral("Armed");
    QTest::newRow("rover-existing-armed") << 10 << true << false << false << QStringLiteral("Armed");
    QTest::newRow("rover-existing-flying-flag") << 10 << true << true << false << QStringLiteral("Flying");
    QTest::newRow("multirotor-flying") << 2 << true << true << false << QStringLiteral("Flying");
    QTest::newRow("multirotor-landing") << 2 << true << false << true << QStringLiteral("Landing");
    QTest::newRow("fixed-wing-flying") << 1 << true << true << false << QStringLiteral("Flying");
    QTest::newRow("vtol-flying") << 20 << true << true << false << QStringLiteral("Flying");
}

void BoatStatusTest::_status()
{
    QFETCH(int, type);
    QFETCH(bool, armed);
    QFETCH(bool, flying);
    QFETCH(bool, landing);
    QFETCH(QString, text);
    ignoreLogMessage("qt.qpa.fonts", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Populating font family aliases")));
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/unittest/BoatStatusHarness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> harness(component.create());
    QVERIFY2(harness, qPrintable(component.errorString()));
    auto* vehicle = harness->property("vehicle").value<QObject*>();
    auto* indicator = harness->property("indicator").value<QObject*>();
    QVERIFY(vehicle && indicator);
    auto* label = indicator->findChild<QObject*>(QStringLiteral("mainStatus_text"));
    QVERIFY(label);
    vehicle->setProperty("kind", type);
    vehicle->setProperty("armed", armed);
    vehicle->setProperty("flying", flying);
    vehicle->setProperty("landing", landing);
    QCOMPARE(label->property("text").toString(), text);
    QCOMPARE(indicator->property("_vehicleInAir").toBool(), type != 11 && (flying || landing));
    harness->setProperty("_communicationLost", true);
    QCOMPARE(label->property("text").toString(), QStringLiteral("Comms Lost"));
    harness->setProperty("_communicationLost", false);
    QCOMPARE(label->property("text").toString(), text);
    indicator->setProperty("_activeVehicle", QVariant::fromValue<QObject*>(nullptr));
    QVERIFY(label->property("text").toString().startsWith(QStringLiteral("Disconnected")));
}
