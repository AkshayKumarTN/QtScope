#include "scpSimulatedAcquisitionSource.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <atomic>

SimulatedAcquisitionWorker::SimulatedAcquisitionWorker(scpSimulatedAcquisitionSource* parent)
    : m_parent(parent),
      m_rng(std::random_device{}()),
      m_randomDist(-1.0f, 1.0f),
      m_noiseDist(0.0f, 1.0f) {
}

void SimulatedAcquisitionWorker::stop() {
    m_shouldStop = true;
    wait(1000);  // Wait up to 1 second for thread to finish
}

void SimulatedAcquisitionWorker::run() {
    const int sampleRate = m_parent->m_sampleRate;
    const double samplePeriod = 1.0 / sampleRate;
    const int chunkSize = sampleRate / 100;  // Generate ~10ms chunks at a time
    QVector<float> chunk(chunkSize);
    
    double phase = 0.0;
    double frequency = m_parent->m_frequencyHz;
    float noiseLevel = m_parent->m_noiseLevel;
    scpSimulatedAcquisitionSource::WaveformType waveformType = m_parent->m_waveformType;

    while (!m_shouldStop) {
        // Lock to read parameters
        {
            QMutexLocker lock(&m_parent->m_bufferMutex);
            frequency = m_parent->m_frequencyHz;
            noiseLevel = m_parent->m_noiseLevel;
            waveformType = m_parent->m_waveformType;
        }

        // Generate chunk
        for (int i = 0; i < chunkSize; ++i) {
            float sample = 0.0f;

            if (waveformType == scpSimulatedAcquisitionSource::NoisySine) {
                // Generate sine wave with noise
                float sineValue = std::sin(2.0 * M_PI * phase);
                float noise = m_noiseDist(m_rng) * noiseLevel;
                sample = sineValue + noise;
                phase += frequency * samplePeriod;
                if (phase >= 1.0) phase -= 1.0;
            } else {  // Random
                // Generate random analog-like data
                sample = m_randomDist(m_rng);
            }

            chunk[i] = sample;
        }

        // Send samples to parent
        m_parent->receiveSamples(chunk.data(), chunkSize);

        // Sleep for approximately chunk duration
        QThread::usleep(static_cast<unsigned long>(chunkSize * samplePeriod * 1000000));
    }
}

scpSimulatedAcquisitionSource::scpSimulatedAcquisitionSource(QObject* parent)
    : scpDataSource(parent) {
    m_bufferSize = m_sampleRate * kBufferSeconds;
    m_buffer.resize(m_bufferSize);
    m_buffer.fill(0.0f);
    m_worker = new SimulatedAcquisitionWorker(this);
}

scpSimulatedAcquisitionSource::~scpSimulatedAcquisitionSource() {
    stop();
    if (m_worker) {
        m_worker->stop();
        delete m_worker;
        m_worker = nullptr;
    }
}

bool scpSimulatedAcquisitionSource::start() {
    // Check if already running (without locking for initial check)
    if (m_running && m_worker && m_worker->isRunning()) {
        return true;
    }
    
    // If worker thread has finished (from previous stop), create a new one
    // Do this outside the mutex to avoid deadlock
    if (m_worker && m_worker->isFinished()) {
        m_worker->wait(); // Wait for thread to fully finish
        delete m_worker;
        m_worker = new SimulatedAcquisitionWorker(this);
    }
    
    // Now lock and update state
    {
        QMutexLocker lock(&m_bufferMutex);
        
        // Double-check after locking
        if (m_running && m_worker && m_worker->isRunning()) {
            return true;
        }
        
        // Reset buffer
        m_buffer.fill(0.0f);
        m_bufferWritePos = 0;
        
        m_running = true;
    } // Unlock before starting thread and emitting signal

    // Start worker thread if not running
    if (m_worker && !m_worker->isRunning()) {
        m_worker->start();
    }

    emit stateChanged(true);
    return true;
}

void scpSimulatedAcquisitionSource::stop() {
    if (!m_running) return;

    m_running = false;
    if (m_worker) {
        m_worker->stop();
    }
    emit stateChanged(false);
}

bool scpSimulatedAcquisitionSource::isActive() const {
    // Check m_running first - this is set synchronously when start() is called
    // Don't check isRunning() immediately after start() as there's a race condition
    // where the thread might not have started yet
    if (!m_running || !m_worker) return false;
    // If worker thread has finished/crashed, we're not active
    if (m_worker->isFinished()) return false;
    return true;
}

int scpSimulatedAcquisitionSource::sampleRate() const {
    return m_sampleRate;
}

int scpSimulatedAcquisitionSource::copyRecentSamples(int count, QVector<float>& out) {
    QMutexLocker lock(&m_bufferMutex);
    int n = std::min(count, m_bufferSize);
    out.resize(n);
    
    if (n == 0) return 0;

    int end = m_bufferWritePos;
    int start = (end - n + m_bufferSize) % m_bufferSize;
    
    if (start < end) {
        std::copy(m_buffer.begin() + start, m_buffer.begin() + end, out.begin());
    } else {
        int first = m_bufferSize - start;
        std::copy(m_buffer.begin() + start, m_buffer.end(), out.begin());
        std::copy(m_buffer.begin(), m_buffer.begin() + end, out.begin() + first);
    }
    
    return n;
}

void scpSimulatedAcquisitionSource::setWaveformType(WaveformType type) {
    QMutexLocker lock(&m_bufferMutex);
    m_waveformType = type;
}

void scpSimulatedAcquisitionSource::setFrequency(double hz) {
    QMutexLocker lock(&m_bufferMutex);
    m_frequencyHz = hz;
}

void scpSimulatedAcquisitionSource::setNoiseLevel(float level) {
    QMutexLocker lock(&m_bufferMutex);
    m_noiseLevel = std::clamp(level, 0.0f, 1.0f);
}

scpSimulatedAcquisitionSource::WaveformType scpSimulatedAcquisitionSource::waveformType() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_waveformType;
}

double scpSimulatedAcquisitionSource::frequency() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_frequencyHz;
}

float scpSimulatedAcquisitionSource::noiseLevel() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_noiseLevel;
}

void scpSimulatedAcquisitionSource::receiveSamples(const float* data, int count) {
    if (!data || count <= 0) return;

    QMutexLocker lock(&m_bufferMutex);
    for (int i = 0; i < count; ++i) {
        m_buffer[m_bufferWritePos] = data[i];
        m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
    }

    // Emit signal for real-time updates
    emit samplesReady(data, count);
}

