#pragma once

#include <memory>

#include "UnitTest.h"

class QQmlEngine;

class BoatVirtualJoystickTest : public UnitTest
{
    Q_OBJECT

public:
    ~BoatVirtualJoystickTest() override;

private slots:
    void initTestCase() override;
    void init() override;
    void cleanup() override;
    void _mapping_data();
    void _mapping();
    void _lossAndVisibility_data();
    void _lossAndVisibility();
    void _targetTransitions();
    void _layoutChanges();

private:
    QObject* _part(const char* name) const;
    QObject* _stick(const char* name) const;
    void _poll();
    void _moveSticks();
    void _verifyCancelled();

    std::unique_ptr<QQmlEngine> _engine;
    std::unique_ptr<QObject> _harness;
};
