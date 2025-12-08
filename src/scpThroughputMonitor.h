#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QMutex>

/**
 * @brief Monitors throughput and performance metrics
 * 
 * Tracks:
 * - Bytes per second (read/write)
 * - Samples per second
 * - Drop rates
 * - Latency
 * - Buffer utilization
 * 
 * Provides real-time statistics and alerts on performance issues.
 */
class scpThroughputMonitor : public QObject {
    Q_OBJECT

public:
    explicit scpThroughputMonitor(QObject* parent = nullptr);
    ~scpThroughputMonitor() override;

    // Configuration
    void setUpdateInterval(int ms) { m_updateIntervalMs = ms; }
    int updateInterval() const { return m_updateIntervalMs; }

    void setSampleSize(int bytes) { m_sampleSizeBytes = bytes; }
    int sampleSize() const { return m_sampleSizeBytes; }

    // Start/Stop monitoring
    void start();
    void stop();
    bool isMonitoring() const { return m_timer && m_timer->isActive(); }

    // Record events (thread-safe)
    void recordBytesRead(int bytes);
    void recordBytesWritten(int bytes);
    void recordSamples(int count);
    void recordDropped(int count);
    void recordLatency(int microseconds);

    // Statistics (current values)
    double bytesPerSecondRead() const { return m_currentBytesPerSecondRead; }
    double bytesPerSecondWrite() const { return m_currentBytesPerSecondWrite; }
    double samplesPerSecond() const { return m_currentSamplesPerSecond; }
    double dropRate() const { return m_currentDropRate; }
    double averageLatency() const { return m_currentAvgLatency; }

    // Cumulative statistics (since start)
    qint64 totalBytesRead() const { return m_totalBytesRead; }
    qint64 totalBytesWritten() const { return m_totalBytesWritten; }
    qint64 totalSamples() const { return m_totalSamples; }
    qint64 totalDropped() const { return m_totalDropped; }

    // Reset statistics
    void reset();

    // Get formatted statistics string
    QString getStatisticsString() const;

signals:
    void statisticsUpdated(const QString& stats);
    void throughputAlert(double bytesPerSec, const QString& message);
    void dropRateAlert(double dropRate, const QString& message);

private slots:
    void onUpdateTimer();

private:
    void calculateStatistics();
    void checkAlerts();

    QTimer* m_timer;
    QElapsedTimer m_elapsedTimer;
    int m_updateIntervalMs;
    int m_sampleSizeBytes;

    // Counters (protected by mutex)
    mutable QMutex m_mutex;
    qint64 m_totalBytesRead;
    qint64 m_totalBytesWritten;
    qint64 m_totalSamples;
    qint64 m_totalDropped;
    QQueue<int> m_latencyHistory;  // Recent latency measurements in microseconds
    static const int MAX_LATENCY_HISTORY = 100;

    // Time tracking
    qint64 m_lastBytesRead;
    qint64 m_lastBytesWritten;
    qint64 m_lastSamples;
    qint64 m_lastDropped;
    qint64 m_lastUpdateTime;

    // Current calculated values
    double m_currentBytesPerSecondRead;
    double m_currentBytesPerSecondWrite;
    double m_currentSamplesPerSecond;
    double m_currentDropRate;
    double m_currentAvgLatency;

    // Alert thresholds
    double m_minThroughputBytesPerSec;
    double m_maxDropRate;
};

