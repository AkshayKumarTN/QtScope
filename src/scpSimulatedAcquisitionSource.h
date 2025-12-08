#pragma once
#include "scpDataSource.h"
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <random>

class SimulatedAcquisitionWorker;

class scpSimulatedAcquisitionSource : public scpDataSource {
    Q_OBJECT
public:
    explicit scpSimulatedAcquisitionSource(QObject* parent = nullptr);
    ~scpSimulatedAcquisitionSource() override;

    bool start() override;
    void stop() override;
    bool isActive() const override;
    int sampleRate() const override;
    int copyRecentSamples(int count, QVector<float>& out) override;

    enum WaveformType {
        NoisySine,
        Random
    };

    void setWaveformType(WaveformType type);
    void setFrequency(double hz);  // For noisy sine mode
    void setNoiseLevel(float level);  // 0.0 to 1.0

    WaveformType waveformType() const;
    double frequency() const;
    float noiseLevel() const;

private:
    friend class SimulatedAcquisitionWorker;
    void receiveSamples(const float* data, int count);

    int m_sampleRate = 10000;  // Default 10 kHz
    bool m_running = false;
    WaveformType m_waveformType = NoisySine;
    double m_frequencyHz = 1000.0;
    float m_noiseLevel = 0.1f;

    SimulatedAcquisitionWorker* m_worker = nullptr;
    mutable QMutex m_bufferMutex;  // mutable allows locking in const methods
    QVector<float> m_buffer;
    int m_bufferSize = 0;
    int m_bufferWritePos = 0;

    static constexpr int kBufferSeconds = 1;  // ~1 second of data
};

// Worker thread that generates samples
class SimulatedAcquisitionWorker : public QThread {
    Q_OBJECT
public:
    explicit SimulatedAcquisitionWorker(scpSimulatedAcquisitionSource* parent);
    void stop();

protected:
    void run() override;

private:
    scpSimulatedAcquisitionSource* m_parent;
    std::atomic<bool> m_shouldStop{false};
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_randomDist;
    std::normal_distribution<float> m_noiseDist;
};

