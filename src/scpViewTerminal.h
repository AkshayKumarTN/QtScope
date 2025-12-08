#pragma once
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QTextStream>
#include <QElapsedTimer>
#include "scpView.h"

class scpTerminalController;

class scpViewTerminal : public QObject, public scpView {
    Q_OBJECT
public:
    explicit scpViewTerminal(QObject* parent=nullptr);
    ~scpViewTerminal() override;
    void setSource(class scpDataSource* src) override;
    void setAcquisitionSource(class scpDataSource* src);
    void setGeneratorSource(class scpDataSource* src);
    void setTotalTimeWindowSec(double sec10Div) override { m_timeWindowSec = sec10Div; }
    void setVerticalScale(float unitsPerDiv) override { m_unitsPerDiv = unitsPerDiv; }

    void start();
    void stop();

private slots:
    void onTick();
    void onStdinActivity();
    void onControllerStartRequested();
    void onControllerStopRequested();
    void onControllerQuitRequested();
    void onControllerViewUpdateNeeded();

private:
    void printFrame(const QVector<float>& samples);
    void printHelp();

    class scpDataSource* m_source = nullptr;
    class scpDataSource* m_acquisitionSource = nullptr;  // For combined mode
    class scpDataSource* m_generatorSource = nullptr;     // For combined mode
    class scpTerminalController* m_controller = nullptr;  // Controller for command processing
    QTimer m_timer;
    QTimer* m_sampleForTimer = nullptr;  // Timer for sampleFor= command (managed by controller)
    double m_timeWindowSec = 0.5; // 500ms across screen
    float m_unitsPerDiv = 1.0f;
    bool m_useAnsi = false;
    bool m_isTyping = false;  // Flag to pause display when user is typing
    QTextStream m_out;
    QTextStream m_in;

#ifdef Q_OS_UNIX
    class QSocketNotifier* m_stdinNotifier = nullptr;
    class QTimer* m_stdinPollTimer = nullptr;  // Polling timer for input
#elif defined(Q_OS_WIN)
    class QTimer* m_stdinPollTimer = nullptr;
#endif
};
