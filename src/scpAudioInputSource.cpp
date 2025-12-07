#include "scpAudioInputSource.h"
#include <QtEndian>
#include <QDebug>

static constexpr int kDefaultSampleRate = 44100;
static constexpr int kChannels = 1;
static constexpr int kBufferSeconds = 5; // keep last 5 seconds

scpAudioInputSource::scpAudioInputSource(QObject* parent)
    : scpDataSource(parent) {
    // Request a reasonable mono format; fall back to preferred if not supported.
    QAudioFormat req;
    req.setSampleRate(kDefaultSampleRate);
    req.setChannelCount(kChannels);
    req.setSampleFormat(QAudioFormat::Int16); // widely supported

    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!dev.isFormatSupported(req)) {
        m_format = dev.preferredFormat();
        m_format.setChannelCount(1); // ensure mono
    } else {
        m_format = req;
    }

    m_bufferSize = m_format.sampleRate() * kBufferSeconds;
    m_buffer.resize(m_bufferSize);
    m_buffer.fill(0.f);
}

scpAudioInputSource::~scpAudioInputSource() {
    stop();
}

bool scpAudioInputSource::start() {
    if (m_running) return true;
    QAudioDevice dev = QMediaDevices::defaultAudioInput();
    if (!dev.isNull()) {
        m_audio = new QAudioSource(dev, m_format, this);
        m_device = m_audio->start();
        if (!m_device) {
            qWarning() << "Failed to start audio input";
            delete m_audio; m_audio = nullptr;
            return false;
        }
        connect(m_device, &QIODevice::readyRead, this, &scpAudioInputSource::onReadyRead);
        m_running = true;
        emit stateChanged(true);
        return true;
    }
    return false;
}

void scpAudioInputSource::stop() {
    if (!m_running) return;
    if (m_audio) {
        disconnect(m_device, nullptr, this, nullptr);
        m_audio->stop();
        m_audio->deleteLater();
        m_audio = nullptr;
        m_device = nullptr;
    }
    m_running = false;
    emit stateChanged(false);
}

int scpAudioInputSource::copyRecentSamples(int count, QVector<float>& out) {
    QMutexLocker lock(&m_mutex);
    if (count <= 0) return 0;
    int n = qMin(count, m_bufferSize);
    out.resize(n);
    // Copy circularly from writePos - n
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

void scpAudioInputSource::onReadyRead() {
    if (!m_device) return;
    QByteArray data = m_device->readAll();
    if (data.isEmpty()) return;
    appendSamplesFromBytes(data.constData(), data.size());
}

void scpAudioInputSource::appendSamplesFromBytes(const char* data, int bytes) {
    QMutexLocker lock(&m_mutex);

    // Support Int16 and Float
    if (m_format.sampleFormat() == QAudioFormat::Int16) {
        int16_t const* p = reinterpret_cast<const int16_t*>(data);
        int frames = bytes / sizeof(int16_t) / m_format.channelCount();
        for (int i = 0; i < frames; ++i) {
            // Mix down to mono by taking first channel
            float s = static_cast<float>(p[i * m_format.channelCount()]) / 32768.0f;
            m_buffer[m_bufferWritePos] = s;
            m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
        }
    } else if (m_format.sampleFormat() == QAudioFormat::Float) {
        float const* p = reinterpret_cast<const float*>(data);
        int frames = bytes / sizeof(float) / m_format.channelCount();
        for (int i = 0; i < frames; ++i) {
            float s = p[i * m_format.channelCount()];
            m_buffer[m_bufferWritePos] = s;
            m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
        }
    } else if (m_format.sampleFormat() == QAudioFormat::Int32) {
        int32_t const* p = reinterpret_cast<const int32_t*>(data);
        int frames = bytes / sizeof(int32_t) / m_format.channelCount();
        for (int i = 0; i < frames; ++i) {
            float s = static_cast<float>(p[i * m_format.channelCount()]) / 2147483648.0f;
            m_buffer[m_bufferWritePos] = s;
            m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
        }
    } else if (m_format.sampleFormat() == QAudioFormat::UInt8) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
        int frames = bytes / sizeof(uint8_t) / m_format.channelCount();
        for (int i = 0; i < frames; ++i) {
            float s = (static_cast<float>(p[i * m_format.channelCount()]) - 128.0f) / 128.0f;
            m_buffer[m_bufferWritePos] = s;
            m_bufferWritePos = (m_bufferWritePos + 1) % m_bufferSize;
        }
    }
}
