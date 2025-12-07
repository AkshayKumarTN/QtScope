#pragma once
#include "scpDataSource.h"
#include <QTimer>
#include <cmath>

class scpSignalGeneratorSource : public scpDataSource {
    Q_OBJECT
public:
    explicit scpSignalGeneratorSource(QObject* parent = nullptr);
    ~scpSignalGeneratorSource() override;

    bool start() override;
    void stop() override;
    bool isActive() const override { return m_running; }
    int sampleRate() const override { return m_sampleRate; }
    int copyRecentSamples(int count, QVector<float>& out) override;

    void setFrequency(double hz);
    double frequency() const { return m_freqHz; }

private slots:
    void onTick();

private:
    int m_sampleRate = 44100;
    bool m_running = false;
    QTimer m_timer;
    double m_freqHz = 440.0;
    double m_phase = 0.0;

    QVector<float> m_buffer;
    int m_bufferWritePos = 0;
    int m_bufferSize = 0;
    QMutex m_mutex;
};
