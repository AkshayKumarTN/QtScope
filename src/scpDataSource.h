#pragma once
#include <QObject>
#include <QVector>
#include <QMutex>

// Abstract base class for oscilloscope data sources
class scpDataSource : public QObject {
    Q_OBJECT
public:
    explicit scpDataSource(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~scpDataSource() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isActive() const = 0;
    virtual int sampleRate() const = 0;

    // Copies up to 'count' most-recent samples into 'out'
    virtual int copyRecentSamples(int count, QVector<float>& out) = 0;

signals:
    void stateChanged(bool running);

    // NEW: allows sources (audio, generator, message waves) to send sample data
    void samplesReady(const float* data, int count);
};
