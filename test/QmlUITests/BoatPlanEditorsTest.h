#pragma once

#include "BaseClasses/MissionTest.h"

class BoatPlanEditorsTest : public OfflineMissionTest
{
    Q_OBJECT

private slots:
    void initTestCase() override;
    void init() override;
    void _editors_data();
    void _editors();
};
