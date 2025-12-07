#include "scpSignalGeneratorSource.h"
#include <algorithm>

static constexpr int kGenBufferSeconds = 5;

scpSignalGeneratorSource::scpSignalGeneratorSource(QObject* parent)
    : scpDataSource(parent) {
    m_bufferSize = m_sampleRate * kGenBufferSeconds;
    m_buffer.resize(m_bufferSize);
    m_buffer.fill(0.f);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(10); // generate in small chunks (~10ms)
    QObject::connect(&m_timer, &QTimer::timeout, this, &scpSignalGeneratorSource::onTick);
}

scpSignalGeneratorSource::~scpSignalGeneratorSource() {
    stop();
}

bool scpSignalGeneratorSource::start() {
    if (m_running) return true;
    m_timer.start();
    m_running = true;
    emit stateChanged(true);
    return true;
}

void scpSignalGeneratorSource::stop() {
    if (!m_running) return;
    m_timer.stop();
    m_running = false;
    emit stateChanged(false);
}

int scpSignalGeneratorSource::copyRecentSamples(int count, QVector<float>& out) {
    QMutexLocker lock(&m_mutex);
    int n = std::min(count, m_bufferSize);
    out.resize(n);
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

void scpSignalGeneratorSource::setFrequency(double hz) {
    QMutexLocker lock(&m_mutex);
    m_freqHz = hz;
}

void scpSignalGeneratorSource::onTick() {
    const double dt = m_timer.interval() / 1000.0;
    const int frames = static_cast<int>(m_sampleRate * dt);
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < frames; ++i) {
        double s = std::sin(2.0 * M_PI * m_phase);
        m_phase += m_freqHz / m_sampleRate;
        if (m_phase >= 1.0) m_phase -= 1.0;
        m_buffer[m_bufferWritePos] = static_cast<float>(s);
        m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
    }
}
