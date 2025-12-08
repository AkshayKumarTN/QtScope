#pragma once
#include "scpDataSource.h"
#include <QThread>
#include <QMutex>
#include <atomic>

class SimulatedGeneratorWorker;

class scpSimulatedGeneratorSource : public scpDataSource {
    Q_OBJECT
public:
    explicit scpSimulatedGeneratorSource(QObject* parent = nullptr);
    ~scpSimulatedGeneratorSource() override;

    bool start() override;
    void stop() override;
    bool isActive() const override;
    int sampleRate() const override;
    int copyRecentSamples(int count, QVector<float>& out) override;

    enum WaveformType {
        Sine,
        Square,
        Triangle
    };

    void setWaveformType(WaveformType type);
    void setFrequency(double hz);
    void setAmplitude(float amp);
    void setOffset(float offset);

    WaveformType waveformType() const;
    double frequency() const;
    float amplitude() const;
    float offset() const;

private:
    friend class SimulatedGeneratorWorker;
    void receiveSamples(const float* data, int count);

    int m_sampleRate = 44100;  // Default 44.1 kHz
    bool m_running = false;
    WaveformType m_waveformType = Sine;
    double m_frequencyHz = 440.0;
    float m_amplitude = 1.0f;
    float m_offset = 0.0f;

    SimulatedGeneratorWorker* m_worker = nullptr;
    mutable QMutex m_bufferMutex;  // mutable allows locking in const methods
    QVector<float> m_buffer;
    int m_bufferSize = 0;
    int m_bufferWritePos = 0;

    static constexpr int kBufferSeconds = 1;  // ~1 second of data
};

// Worker thread that generates samples
class SimulatedGeneratorWorker : public QThread {
    Q_OBJECT
public:
    explicit SimulatedGeneratorWorker(scpSimulatedGeneratorSource* parent);
    void stop();

protected:
    void run() override;

private:
    scpSimulatedGeneratorSource* m_parent;
    std::atomic<bool> m_shouldStop{false};
};

