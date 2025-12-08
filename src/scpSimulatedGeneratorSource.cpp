#include "scpSimulatedGeneratorSource.h"
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <atomic>

SimulatedGeneratorWorker::SimulatedGeneratorWorker(scpSimulatedGeneratorSource* parent)
    : m_parent(parent) {
}

void SimulatedGeneratorWorker::stop() {
    m_shouldStop = true;
    wait(1000);  // Wait up to 1 second for thread to finish
}

void SimulatedGeneratorWorker::run() {
    const int sampleRate = m_parent->m_sampleRate;
    const double samplePeriod = 1.0 / sampleRate;
    const int chunkSize = sampleRate / 100;  // Generate ~10ms chunks at a time
    QVector<float> chunk(chunkSize);
    
    double phase = 0.0;
    double frequency = m_parent->m_frequencyHz;
    float amplitude = m_parent->m_amplitude;
    float offset = m_parent->m_offset;
    scpSimulatedGeneratorSource::WaveformType waveformType = m_parent->m_waveformType;

    while (!m_shouldStop) {
        // Lock to read parameters
        {
            QMutexLocker lock(&m_parent->m_bufferMutex);
            frequency = m_parent->m_frequencyHz;
            amplitude = m_parent->m_amplitude;
            offset = m_parent->m_offset;
            waveformType = m_parent->m_waveformType;
        }

        // Generate chunk
        for (int i = 0; i < chunkSize; ++i) {
            float sample = 0.0f;

            switch (waveformType) {
                case scpSimulatedGeneratorSource::Sine: {
                    sample = std::sin(2.0 * M_PI * phase);
                    break;
                }
                case scpSimulatedGeneratorSource::Square: {
                    sample = (phase < 0.5) ? 1.0f : -1.0f;
                    break;
                }
                case scpSimulatedGeneratorSource::Triangle: {
                    if (phase < 0.5) {
                        sample = 4.0f * phase - 1.0f;  // -1 to +1 over first half
                    } else {
                        sample = 3.0f - 4.0f * phase;  // +1 to -1 over second half
                    }
                    break;
                }
            }

            // Apply amplitude and offset
            sample = sample * amplitude + offset;
            chunk[i] = sample;

            // Update phase
            phase += frequency * samplePeriod;
            if (phase >= 1.0) phase -= 1.0;
        }

        // Send samples to parent
        m_parent->receiveSamples(chunk.data(), chunkSize);

        // Sleep for approximately chunk duration
        QThread::usleep(static_cast<unsigned long>(chunkSize * samplePeriod * 1000000));
    }
}

scpSimulatedGeneratorSource::scpSimulatedGeneratorSource(QObject* parent)
    : scpDataSource(parent) {
    m_bufferSize = m_sampleRate * kBufferSeconds;
    m_buffer.resize(m_bufferSize);
    m_buffer.fill(0.0f);
    m_worker = new SimulatedGeneratorWorker(this);
}

scpSimulatedGeneratorSource::~scpSimulatedGeneratorSource() {
    stop();
    if (m_worker) {
        m_worker->stop();
        delete m_worker;
        m_worker = nullptr;
    }
}

bool scpSimulatedGeneratorSource::start() {
    // Check if already running (without locking for initial check)
    if (m_running && m_worker && m_worker->isRunning()) {
        return true;
    }
    
    // If worker thread has finished (from previous stop), create a new one
    // Do this outside the mutex to avoid deadlock
    if (m_worker && m_worker->isFinished()) {
        m_worker->wait(); // Wait for thread to fully finish
        delete m_worker;
        m_worker = new SimulatedGeneratorWorker(this);
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

void scpSimulatedGeneratorSource::stop() {
    if (!m_running) return;

    m_running = false;
    if (m_worker) {
        m_worker->stop();
    }
    emit stateChanged(false);
}

bool scpSimulatedGeneratorSource::isActive() const {
    // Check m_running first - this is set synchronously when start() is called
    // Don't check isRunning() immediately after start() as there's a race condition
    // where the thread might not have started yet
    if (!m_running || !m_worker) return false;
    // If worker thread has finished/crashed, we're not active
    if (m_worker->isFinished()) return false;
    return true;
}

int scpSimulatedGeneratorSource::sampleRate() const {
    return m_sampleRate;
}

int scpSimulatedGeneratorSource::copyRecentSamples(int count, QVector<float>& out) {
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

void scpSimulatedGeneratorSource::setWaveformType(WaveformType type) {
    QMutexLocker lock(&m_bufferMutex);
    m_waveformType = type;
}

void scpSimulatedGeneratorSource::setFrequency(double hz) {
    QMutexLocker lock(&m_bufferMutex);
    m_frequencyHz = hz;
}

void scpSimulatedGeneratorSource::setAmplitude(float amp) {
    QMutexLocker lock(&m_bufferMutex);
    m_amplitude = amp;
}

void scpSimulatedGeneratorSource::setOffset(float offset) {
    QMutexLocker lock(&m_bufferMutex);
    m_offset = offset;
}

scpSimulatedGeneratorSource::WaveformType scpSimulatedGeneratorSource::waveformType() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_waveformType;
}

double scpSimulatedGeneratorSource::frequency() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_frequencyHz;
}

float scpSimulatedGeneratorSource::amplitude() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_amplitude;
}

float scpSimulatedGeneratorSource::offset() const {
    QMutexLocker lock(&m_bufferMutex);
    return m_offset;
}

void scpSimulatedGeneratorSource::receiveSamples(const float* data, int count) {
    if (!data || count <= 0) return;

    QMutexLocker lock(&m_bufferMutex);
    for (int i = 0; i < count; ++i) {
        m_buffer[m_bufferWritePos] = data[i];
        m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
    }

    // Emit signal for real-time updates
    emit samplesReady(data, count);
}

