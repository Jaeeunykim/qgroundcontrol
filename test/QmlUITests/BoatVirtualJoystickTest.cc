#include "BoatVirtualJoystickTest.h"

#include <QtCore/QRegularExpression>
#include <QtQml/QJSValue>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>

#include "AppSettings.h"
#include "ColoredSvgImageProvider.h"
#include "SettingsManager.h"

UT_REGISTER_TEST(BoatVirtualJoystickTest, TestLabel::Integration, TestLabel::Vehicle)

BoatVirtualJoystickTest::~BoatVirtualJoystickTest() = default;

void BoatVirtualJoystickTest::initTestCase()
{
    UnitTest::initTestCase();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void BoatVirtualJoystickTest::init()
{
    UnitTest::init();
    ignoreLogMessage("qt.qpa.fonts", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Populating font family aliases")));
    SettingsManager::instance()->appSettings()->virtualJoystick()->setRawValue(true);
    _engine = std::make_unique<QQmlEngine>();
    _engine->addImportPath(QStringLiteral("qrc:/qml"));
    _engine->addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(_engine.get(), QUrl(QStringLiteral("qrc:/unittest/BoatVirtualJoystickHarness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    _harness.reset(component.create());
    QVERIFY2(_harness, qPrintable(component.errorString()));
    QVERIFY(_part("controls"));
    QVERIFY(_stick("leftStick"));
    QVERIFY(_stick("rightStick"));
}

void BoatVirtualJoystickTest::cleanup()
{
    _harness.reset();
    _engine.reset();
    UnitTest::cleanup();
}

QObject* BoatVirtualJoystickTest::_part(const char* name) const
{
    return _harness->property(name).value<QObject*>();
}

QObject* BoatVirtualJoystickTest::_stick(const char* name) const
{
    return _part("controls")->findChild<QObject*>(QStringLiteral("virtualJoystick_%1").arg(QLatin1String(name)));
}

void BoatVirtualJoystickTest::_poll()
{
    auto* timer = _part("controls")->findChild<QObject*>(QStringLiteral("virtualJoystick_poll"));
    QVERIFY(timer);
    QSignalSpy triggered(timer, SIGNAL(triggered()));
    QVERIFY(triggered.isValid());
    QVERIFY(triggered.wait(TestTimeout::shortMs()));
}

void BoatVirtualJoystickTest::_moveSticks()
{
    auto* left = _stick("leftStick");
    auto* right = _stick("rightStick");
    for (QObject* stick : {left, right}) {
        const QVariantList points{QVariantMap{{QStringLiteral("x"), stick->property("width").toDouble() / 2},
                                              {QStringLiteral("y"), stick->property("height").toDouble() / 2}}};
        QVERIFY(QMetaObject::invokeMethod(stick, "thumbDown", Q_ARG(QVariant, points)));
    }
    left->setProperty("xAxis", 0.25);
    left->setProperty("yAxis", -0.5);
    right->setProperty("xAxis", -0.75);
    right->setProperty("yAxis", 0.125);
    left->setProperty("touchActive", true);
}

void BoatVirtualJoystickTest::_verifyCancelled()
{
    for (const char* name : {"leftStick", "rightStick"}) {
        auto* stick = _stick(name);
        QCOMPARE(stick->property("xAxis").toDouble(), 0.0);
        QCOMPARE(stick->property("yAxis").toDouble(), 0.0);
        QVERIFY(!stick->property("touchActive").toBool());
        QVERIFY(!stick->property("_processTouchPoints").toBool());
    }
}

void BoatVirtualJoystickTest::_mapping_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("leftHanded");
    QTest::addColumn<bool>("autoCenter");
    for (int type : {11, 10, 2, 1, 20}) {
        for (bool leftHanded : {false, true}) {
            for (bool autoCenter : {false, true}) {
                const auto name =
                    QStringLiteral("type%1-left%2-center%3").arg(type).arg(leftHanded).arg(autoCenter).toLatin1();
                QTest::newRow(name.constData()) << type << leftHanded << autoCenter;
            }
        }
    }
}

void BoatVirtualJoystickTest::_mapping()
{
    QFETCH(int, type);
    QFETCH(bool, leftHanded);
    QFETCH(bool, autoCenter);
    auto* vehicle = _part("vehicleA");
    vehicle->setProperty("kind", type);
    _harness->setProperty("leftHandedMode", leftHanded);
    _harness->setProperty("autoCenterThrottle", autoCenter);
    _poll();
    QCOMPARE(vehicle->property("commandCount").toInt() > 0, type != 11);
    _moveSticks();
    _poll();
    const QVariantList expected =
        leftHanded ? QVariantList{0.25, -0.5, -0.75, 0.125} : QVariantList{-0.75, 0.125, 0.25, -0.5};
    QCOMPARE(vehicle->property("lastAxes").value<QJSValue>().toVariant().toList(), expected);
    _stick("leftStick")->setProperty("touchActive", false);
    const int previous = vehicle->property("commandCount").toInt();
    _poll();
    QVERIFY(vehicle->property("commandCount").toInt() > previous);
}

void BoatVirtualJoystickTest::_lossAndVisibility_data()
{
    QTest::addColumn<QString>("event");
    for (const char* event : {"communicationLost", "visible", "enabled", "initialConnectComplete", "setting"}) {
        QTest::newRow(event) << QString::fromLatin1(event);
    }
}

void BoatVirtualJoystickTest::_lossAndVisibility()
{
    QFETCH(QString, event);
    auto* vehicle = _part("vehicleA");
    auto* controls = _part("controls");
    auto* linkManager = vehicle->property("vehicleLinkManager").value<QObject*>();
    _moveSticks();
    _poll();
    QVERIFY(vehicle->property("commandCount").toInt() > 0);
    const auto interrupt = [&](bool blocked) {
        if (event == QStringLiteral("communicationLost")) {
            linkManager->setProperty("communicationLost", blocked);
        } else if (event == QStringLiteral("initialConnectComplete")) {
            vehicle->setProperty("initialConnectComplete", !blocked);
        } else if (event == QStringLiteral("setting")) {
            SettingsManager::instance()->appSettings()->virtualJoystick()->setRawValue(!blocked);
        } else {
            controls->setProperty(event.toLatin1().constData(), !blocked);
        }
    };
    interrupt(true);
    _verifyCancelled();
    const int previous = vehicle->property("commandCount").toInt();
    if (event != QStringLiteral("setting")) {
        _poll();
        QCOMPARE(vehicle->property("commandCount").toInt(), previous);
    }
    interrupt(false);
    _poll();
    QCOMPARE(vehicle->property("commandCount").toInt(), previous);
    _moveSticks();
    _poll();
    QVERIFY(vehicle->property("commandCount").toInt() > previous);
}

void BoatVirtualJoystickTest::_targetTransitions()
{
    auto* first = _part("vehicleA");
    auto* second = _part("vehicleB");
    _moveSticks();
    _poll();
    const int previous = first->property("commandCount").toInt();
    _harness->setProperty("selectedVehicle", QVariant::fromValue(second));
    _verifyCancelled();
    _poll();
    QCOMPARE(first->property("commandCount").toInt(), previous);
    QCOMPARE(second->property("commandCount").toInt(), 0);
    _moveSticks();
    _poll();
    QVERIFY(second->property("commandCount").toInt() > 0);
    _harness->setProperty("selectedVehicle", QVariant::fromValue<QObject*>(nullptr));
    _verifyCancelled();
    _poll();
    _harness->setProperty("selectedVehicle", QVariant::fromValue(first));
    _poll();
    QCOMPARE(first->property("commandCount").toInt(), previous);
    first->setProperty("kind", 10);
    _moveSticks();
    _poll();
    const int roverCount = first->property("commandCount").toInt();
    first->setProperty("kind", 11);
    _verifyCancelled();
    _poll();
    QCOMPARE(first->property("commandCount").toInt(), roverCount);

    _harness->setProperty("autoCenterThrottle", true);
    _moveSticks();
    _poll();
    first->setProperty("kind", 2);
    _verifyCancelled();
    // A release from the old touch and a resize must not produce half throttle on the new aircraft.
    for (const char* name : {"leftStick", "rightStick"}) {
        QVERIFY(QMetaObject::invokeMethod(_stick(name), "releaseInput"));
    }
    _harness->setProperty("width", 600);
    _harness->setProperty("height", 200);
    _verifyCancelled();
    _poll();
    QCOMPARE(first->property("lastAxes").value<QJSValue>().toVariant().toList(), QVariantList({0.0, 0.0, 0.0, 0.0}));
}

void BoatVirtualJoystickTest::_layoutChanges()
{
    auto* vehicle = _part("vehicleA");
    _moveSticks();
    _poll();
    const int previous = vehicle->property("commandCount").toInt();
    _harness->setProperty("leftHandedMode", true);
    _verifyCancelled();
    _poll();
    QCOMPARE(vehicle->property("commandCount").toInt(), previous);
    _moveSticks();
    _poll();
    _harness->setProperty("autoCenterThrottle", true);
    const int beforeCenter = vehicle->property("commandCount").toInt();
    _verifyCancelled();
    _poll();
    QCOMPARE(vehicle->property("commandCount").toInt(), beforeCenter);
}
