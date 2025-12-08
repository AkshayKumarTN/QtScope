#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
#include "scpDataSource.h"

class QTextStream;
class scpView;

/**
 * @brief Controller for terminal-based oscilloscope commands
 * 
 * Handles command parsing and coordinates between View (scpViewTerminal) and Model (scpDataSource).
 * This separates command handling logic from display logic for better MVC architecture.
 */
class scpTerminalController : public QObject {
    Q_OBJECT

public:
    explicit scpTerminalController(QObject* parent = nullptr);
    ~scpTerminalController() override;

    // Set the data source (Model)
    void setSource(scpDataSource* source);
    scpDataSource* source() const { return m_source; }
    
    // Set additional sources for combined mode
    void setAcquisitionSource(scpDataSource* source);
    void setGeneratorSource(scpDataSource* source);
    scpDataSource* acquisitionSource() const { return m_acquisitionSource; }
    scpDataSource* generatorSource() const { return m_generatorSource; }

    // Set the view for sending output messages
    void setView(scpView* view);
    scpView* view() const { return m_view; }

    // Set output stream for command responses
    void setOutputStream(QTextStream* stream);
    QTextStream* outputStream() const { return m_out; }

    // Process a command line
    bool processCommand(const QString& line);

    // Timer management for sampleFor command
    QTimer* sampleForTimer() const { return m_sampleForTimer; }

signals:
    void quitRequested();
    void startRequested();
    void stopRequested();
    void viewUpdateNeeded();

private slots:
    void onSampleForTimeout();

private:
    // Command handlers
    bool handleStart(const QString& arg);
    bool handleStop(const QString& arg);
    bool handleCombined(const QString& arg);
    bool handleSampleTime(const QString& arg);
    bool handleSampleFor(const QString& arg);
    bool handleTimebase(const QString& arg);
    bool handleScale(const QString& arg);
    bool handleFrequency(const QString& arg);
    bool handleAmplitude(const QString& arg);
    bool handleOffset(const QString& arg);
    bool handleWaveform(const QString& arg);
    bool handleNoiseLevel(const QString& arg);
    bool handleStatus(const QString& arg);
    bool handleHelp(const QString& arg);

    // Helper methods
    void writeResponse(const QString& message);
    double parseTimeValue(const QString& valueStr, QString& unit);

    scpDataSource* m_source = nullptr;
    scpDataSource* m_acquisitionSource = nullptr;  // For combined mode
    scpDataSource* m_generatorSource = nullptr;     // For combined mode
    bool m_combinedMode = false;  // Track if in combined mode
    scpView* m_view = nullptr;
    QTextStream* m_out = nullptr;
    QTimer* m_sampleForTimer = nullptr;
    int m_sampleForDurationMs = 0;
};

