#include "scpThroughputMonitor.h"
#include <QDebug>
#include <QMutexLocker>
#include <QString>

scpThroughputMonitor::scpThroughputMonitor(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_updateIntervalMs(1000)  // Update every second
    , m_sampleSizeBytes(4)  // Assume 4 bytes per sample (float)
    , m_totalBytesRead(0)
    , m_totalBytesWritten(0)
    , m_totalSamples(0)
    , m_totalDropped(0)
    , m_lastBytesRead(0)
    , m_lastBytesWritten(0)
    , m_lastSamples(0)
    , m_lastDropped(0)
    , m_lastUpdateTime(0)
    , m_currentBytesPerSecondRead(0.0)
    , m_currentBytesPerSecondWrite(0.0)
    , m_currentSamplesPerSecond(0.0)
    , m_currentDropRate(0.0)
    , m_currentAvgLatency(0.0)
    , m_minThroughputBytesPerSec(1000.0)  // Alert if below 1 KB/s
    , m_maxDropRate(0.01)  // Alert if drop rate > 1%
{
    connect(m_timer, &QTimer::timeout, this, &scpThroughputMonitor::onUpdateTimer);
}

scpThroughputMonitor::~scpThroughputMonitor() {
    stop();
}

void scpThroughputMonitor::start() {
    reset();
    m_elapsedTimer.start();
    m_lastUpdateTime = m_elapsedTimer.elapsed();
    m_timer->start(m_updateIntervalMs);
}

void scpThroughputMonitor::stop() {
    if (m_timer) {
        m_timer->stop();
    }
}

void scpThroughputMonitor::recordBytesRead(int bytes) {
    QMutexLocker lock(&m_mutex);
    m_totalBytesRead += bytes;
}

void scpThroughputMonitor::recordBytesWritten(int bytes) {
    QMutexLocker lock(&m_mutex);
    m_totalBytesWritten += bytes;
}

void scpThroughputMonitor::recordSamples(int count) {
    QMutexLocker lock(&m_mutex);
    m_totalSamples += count;
}

void scpThroughputMonitor::recordDropped(int count) {
    QMutexLocker lock(&m_mutex);
    m_totalDropped += count;
}

void scpThroughputMonitor::recordLatency(int microseconds) {
    QMutexLocker lock(&m_mutex);
    m_latencyHistory.enqueue(microseconds);
    while (m_latencyHistory.size() > MAX_LATENCY_HISTORY) {
        m_latencyHistory.dequeue();
    }
}

void scpThroughputMonitor::reset() {
    QMutexLocker lock(&m_mutex);
    m_totalBytesRead = 0;
    m_totalBytesWritten = 0;
    m_totalSamples = 0;
    m_totalDropped = 0;
    m_lastBytesRead = 0;
    m_lastBytesWritten = 0;
    m_lastSamples = 0;
    m_lastDropped = 0;
    m_latencyHistory.clear();
    m_currentBytesPerSecondRead = 0.0;
    m_currentBytesPerSecondWrite = 0.0;
    m_currentSamplesPerSecond = 0.0;
    m_currentDropRate = 0.0;
    m_currentAvgLatency = 0.0;
}

QString scpThroughputMonitor::getStatisticsString() const {
    QMutexLocker lock(&m_mutex);
    
    QString stats;
    stats += QString("=== Throughput Statistics ===\n");
    stats += QString("Read:  %1 bytes/sec  (total: %2 bytes)\n")
             .arg(m_currentBytesPerSecondRead, 0, 'f', 1)
             .arg(m_totalBytesRead);
    stats += QString("Write: %1 bytes/sec  (total: %2 bytes)\n")
             .arg(m_currentBytesPerSecondWrite, 0, 'f', 1)
             .arg(m_totalBytesWritten);
    stats += QString("Samples: %1 samples/sec  (total: %2)\n")
             .arg(m_currentSamplesPerSecond, 0, 'f', 1)
             .arg(m_totalSamples);
    
    if (m_totalSamples > 0) {
        double dropPercent = (m_currentDropRate * 100.0);
        stats += QString("Drop Rate: %1%%  (dropped: %2)\n")
                 .arg(dropPercent, 0, 'f', 2)
                 .arg(m_totalDropped);
    }
    
    if (!m_latencyHistory.isEmpty()) {
        stats += QString("Avg Latency: %1 μs\n")
                 .arg(m_currentAvgLatency, 0, 'f', 1);
    }
    
    return stats;
}

void scpThroughputMonitor::onUpdateTimer() {
    calculateStatistics();
    checkAlerts();
    emit statisticsUpdated(getStatisticsString());
}

void scpThroughputMonitor::calculateStatistics() {
    QMutexLocker lock(&m_mutex);
    
    qint64 currentTime = m_elapsedTimer.elapsed();
    qint64 timeDelta = currentTime - m_lastUpdateTime;
    
    if (timeDelta <= 0) return;  // Avoid division by zero
    
    double timeDeltaSec = timeDelta / 1000.0;
    
    // Calculate rates
    qint64 bytesReadDelta = m_totalBytesRead - m_lastBytesRead;
    qint64 bytesWrittenDelta = m_totalBytesWritten - m_lastBytesWritten;
    qint64 samplesDelta = m_totalSamples - m_lastSamples;
    qint64 droppedDelta = m_totalDropped - m_lastDropped;
    
    m_currentBytesPerSecondRead = bytesReadDelta / timeDeltaSec;
    m_currentBytesPerSecondWrite = bytesWrittenDelta / timeDeltaSec;
    m_currentSamplesPerSecond = samplesDelta / timeDeltaSec;
    
    // Calculate drop rate
    qint64 totalProcessed = samplesDelta + droppedDelta;
    if (totalProcessed > 0) {
        m_currentDropRate = static_cast<double>(droppedDelta) / totalProcessed;
    } else {
        m_currentDropRate = 0.0;
    }
    
    // Calculate average latency
    if (!m_latencyHistory.isEmpty()) {
        qint64 sum = 0;
        for (int latency : m_latencyHistory) {
            sum += latency;
        }
        m_currentAvgLatency = static_cast<double>(sum) / m_latencyHistory.size();
    } else {
        m_currentAvgLatency = 0.0;
    }
    
    // Update last values
    m_lastBytesRead = m_totalBytesRead;
    m_lastBytesWritten = m_totalBytesWritten;
    m_lastSamples = m_totalSamples;
    m_lastDropped = m_totalDropped;
    m_lastUpdateTime = currentTime;
}

void scpThroughputMonitor::checkAlerts() {
    QMutexLocker lock(&m_mutex);
    
    // Check throughput alert
    double totalThroughput = m_currentBytesPerSecondRead + m_currentBytesPerSecondWrite;
    if (totalThroughput > 0 && totalThroughput < m_minThroughputBytesPerSec) {
        emit throughputAlert(totalThroughput, 
                           QString("Low throughput detected: %1 bytes/sec").arg(totalThroughput, 0, 'f', 1));
    }
    
    // Check drop rate alert
    if (m_currentDropRate > m_maxDropRate) {
        emit dropRateAlert(m_currentDropRate,
                          QString("High drop rate detected: %1%%").arg(m_currentDropRate * 100.0, 0, 'f', 2));
    }
}

