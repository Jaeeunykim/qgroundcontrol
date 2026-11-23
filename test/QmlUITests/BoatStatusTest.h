#pragma once

#include "UnitTest.h"

class BoatStatusTest : public UnitTest
{
    Q_OBJECT

private slots:
    void initTestCase() override;
    void _status_data();
    void _status();
};
