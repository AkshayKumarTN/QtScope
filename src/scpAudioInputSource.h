#pragma once
#include "scpDataSource.h"
#include <QAudioSource>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QIODevice>
#include <QByteArray>
#include <QTimer>

class scpAudioInputSource : public scpDataSource {
    Q_OBJECT
public:
    explicit scpAudioInputSource(QObject* parent = nullptr);
    ~scpAudioInputSource() override;

    bool start() override;
    void stop() override;
    bool isActive() const override { return m_running; }
    int sampleRate() const override { return m_format.sampleRate(); }
    int copyRecentSamples(int count, QVector<float>& out) override;

private slots:
    void onReadyRead();

private:
    void appendSamplesFromBytes(const char* data, int bytes);

    QAudioFormat m_format;
    QAudioSource* m_audio = nullptr;
    QIODevice* m_device = nullptr;
    bool m_running = false;

    // A simple thread-safe circular buffer of recent samples
    QVector<float> m_buffer;
    int m_bufferWritePos = 0;
    int m_bufferSize = 0;
    QMutex m_mutex;
};
