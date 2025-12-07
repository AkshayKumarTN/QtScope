#pragma once

class scpDataSource;

class scpView {
public:
    virtual ~scpView() = default;

    // total time across the screen (10 divisions)
    virtual void setSource(scpDataSource* src) = 0;
    virtual void setTotalTimeWindowSec(double sec10Div) = 0;
    // arbitrary vertical units per division
    virtual void setVerticalScale(float unitsPerDiv) = 0;
};