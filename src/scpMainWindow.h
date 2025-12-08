#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include "scpScopeView.h"
#include "scpAudioInputSource.h"
#include "scpSignalGeneratorSource.h"
#include "scpMessageWaveSource.h"
#include "scpSimulatedGeneratorSource.h"
#include "scpSimulatedAcquisitionSource.h"
#include "scpDataSource.h"

class scpMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit scpMainWindow(QWidget* parent = nullptr);
    ~scpMainWindow() override;

    // Set the data source (audio, generator, FTDI, message)
    void setSource(scpDataSource* source);

    // Show message in GUI
    void showMessage(const QString& message);

private slots:
    void onSourceChanged(int idx);
    void onStartStop();
    void onTimebaseChanged(int idx);
    void onScaleChanged(int idx);
    void onGenFreqChanged(double f);
    void onWaveformTypeChanged(int idx);
    void onGenAmplitudeChanged(double amp);
    void onGenOffsetChanged(double offset);
    void onSendMessage();  // NEW: handle send button

private:
    void buildUi();

    // UI elements
    scpScopeView* m_view = nullptr;
    QWidget* m_controls = nullptr;
    QComboBox* m_sourceCombo = nullptr;
    QPushButton* m_startStop = nullptr;
    QComboBox* m_timebaseCombo = nullptr;
    QComboBox* m_scaleCombo = nullptr;
    QComboBox* m_waveformCombo = nullptr;
    QDoubleSpinBox* m_genFreq = nullptr;
    QDoubleSpinBox* m_genAmplitude = nullptr;
    QDoubleSpinBox* m_genOffset = nullptr;
    QLabel* m_status = nullptr;       // Status label
    QLabel* m_msgLabel = nullptr;     // Message label for command-line messages
    QLineEdit* m_msgInput = nullptr;  // NEW: text input for message
    QPushButton* m_sendBtn = nullptr; // NEW: send button

    // Data sources
    scpAudioInputSource* m_audio = nullptr;
    scpSignalGeneratorSource* m_gen = nullptr;
    scpMessageWaveSource* m_msgSource = nullptr;
    scpSimulatedGeneratorSource* m_simGen = nullptr;
    scpSimulatedAcquisitionSource* m_simAcq = nullptr;
    scpDataSource* m_current = nullptr;

    bool m_running = false;
};