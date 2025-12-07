#pragma once
#include <QWidget>
#include <QTimer>
#include <QPen>
#include <QMutex>
#include "scpView.h"
#include "scpDataSource.h"

class scpScopeView : public QWidget, public scpView {
    Q_OBJECT
public:
    explicit scpScopeView(QWidget* parent = nullptr);

    void setSource(scpDataSource* src) override;
    void setTotalTimeWindowSec(double sec10Div) override; // total time across the screen (10 divisions)
    void setVerticalScale(float unitsPerDiv) override;

signals:
    void messageChangeRequested(const QString& newMessage);

protected:
    void paintEvent(QPaintEvent* e) override;

private slots:
    void onRefresh();
    void onSamplesReady(const float* data, int count);

private:
    void drawGrid(class QPainter& p);
    void drawWave(class QPainter& p, const QVector<float>& samples);

    scpDataSource* m_source = nullptr;
    QTimer m_timer;
    double m_timeWindowSec = 0.1; // default 100ms across screen
    float m_unitsPerDiv = 1.0f;   // arbitrary vertical units
    
    // Signal-based buffer (alternative to polling)
    QVector<float> m_signalBuffer;
    QMutex m_bufferMutex;
    bool m_useSignalBuffer = false;
};