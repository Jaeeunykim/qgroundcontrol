#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtPositioning/QGeoCoordinate>

#include "LinkConfiguration.h"
#include "QGCMAVLink.h"
#include "UnitTest.h"

class Vehicle;
class QQmlApplicationEngine;

/// Opt-in live PX4 Boat test. Requires an explicitly confirmed simulator endpoint.
class BoatVirtualJoystickSITLTest : public UnitTest
{
    Q_OBJECT

private slots:
    void cleanup() override;
    void _neutralForwardReverse();

private:
    bool _hold(double throttle, int simulationMs, const QString& phase, bool armed = true);
    bool _writeReport() const;

    QQmlApplicationEngine* _engine = nullptr;
    QPointer<Vehicle> _vehicle;
    SharedLinkConfigurationPtr _config;
    QTimer _sender;
    QJsonArray _phases;
    QMap<quint64, QGeoCoordinate> _positions;
    mavlink_message_t _wireMessage{};
    mavlink_status_t _wireStatus{};
    QJsonArray _wireThrottle;
    QString _previousMode;
    QString _reportPath;
    QString _phase;
    double _throttle = 0;
    int _expectedWireThrottle = 500;
    bool _receivedExpectedThrottle = false;
    bool _unexpectedThrottleAfterExpected = false;
    quint64 _simulationMs = 0;
    bool _authorizedVehicle = false;
    bool _completed = false;
};
