#pragma once
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QTextStream>
#include <QElapsedTimer>
#include "scpView.h"

class scpViewTerminal : public QObject, public scpView {
    Q_OBJECT
public:
    explicit scpViewTerminal(QObject* parent=nullptr);
    void setSource(class scpDataSource* src) override;
    void setTotalTimeWindowSec(double sec10Div) override { m_timeWindowSec = sec10Div; }
    void setVerticalScale(float unitsPerDiv) override { m_unitsPerDiv = unitsPerDiv; }

    void start();
    void stop();

private slots:
    void onTick();
    void onStdinActivity();

private:
    void printFrame(const QVector<float>& samples);
    void printHelp();

    class scpDataSource* m_source = nullptr;
    QTimer m_timer;
    double m_timeWindowSec = 0.5; // 500ms across screen
    float m_unitsPerDiv = 1.0f;
    bool m_useAnsi = false;
    QTextStream m_out;
    QTextStream m_in;

#ifdef Q_OS_UNIX
    class QSocketNotifier* m_stdinNotifier = nullptr;
#endif
};
