#pragma once

#include <memory>

#include "UnitTest.h"

class QQmlEngine;

class BoatGuidedActionsTest : public UnitTest
{
    Q_OBJECT

public:
    ~BoatGuidedActionsTest() override;

private slots:
    void initTestCase() override;
    void init() override;
    void cleanup() override;
    void _availability_data();
    void _availability();
    void _pauseRouting_data();
    void _pauseRouting();
    void _confirmationInvalidation_data();
    void _confirmationInvalidation();
    void _rejectUnavailableActions();
    void _missionAndGoto();
    void _resumeAfterMission();
    void _capabilityAndStateChanges();
    void _mixedVehicleMissionMessage();
    void _confirmationIsConsumed();

private:
    QObject* _part(const char* name) const;
    int _action(const char* name) const;
    bool _execute(const char* action, QObject* target = nullptr, bool explicitTarget = false);
    void _confirm(const char* action);

    std::unique_ptr<QQmlEngine> _engine;
    std::unique_ptr<QObject> _harness;
};
