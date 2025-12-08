#pragma once
#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QMutex>
#include "scpFTDIInterface.h"

/**
 * @brief High-level controller for USB write operations
 * 
 * Wraps scpFTDIWriter with additional features:
 * - Queue management with flow control
 * - Auto-reconnect on errors
 * - Buffer overflow protection
 * - Write statistics
 */
class scpUsbWriteController : public QObject {
    Q_OBJECT

public:
    explicit scpUsbWriteController(QObject* parent = nullptr);
    ~scpUsbWriteController() override;

    // Configuration
    void setDevicePath(const QString& path);
    QString devicePath() const { return m_devicePath; }

    void setOutputFrequency(double frequencyHz);
    double outputFrequency() const;

    void setBytesPerWrite(int bytes);
    int bytesPerWrite() const;

    // Queue management
    void setMaxQueueSize(int maxSize) { m_maxQueueSize = maxSize; }
    int maxQueueSize() const { return m_maxQueueSize; }
    int queueSize() const;

    // Auto-reconnect settings
    void setAutoReconnect(bool enable) { m_autoReconnect = enable; }
    bool autoReconnect() const { return m_autoReconnect; }
    void setReconnectDelay(int ms) { m_reconnectDelayMs = ms; }
    int reconnectDelay() const { return m_reconnectDelayMs; }

    // Flow control
    void setFlowControlEnabled(bool enable) { m_flowControlEnabled = enable; }
    bool flowControlEnabled() const { return m_flowControlEnabled; }

    // State
    bool isOpen() const;
    bool isRunning() const;

    // Operations
    bool open();
    void close();
    void start();
    void stop();

    // Queue data for writing (thread-safe)
    bool queueData(const QByteArray& data);
    void clearQueue();

    // Statistics
    qint64 totalBytesWritten() const { return m_totalBytesWritten; }
    int errorCount() const { return m_errorCount; }
    int reconnectCount() const { return m_reconnectCount; }
    int droppedPackets() const { return m_droppedPackets; }

signals:
    void dataWritten(int bytesWritten);
    void writeCompleted();
    void queueEmpty();
    void queueFull();  // Emitted when queue is full and data is dropped
    void errorOccurred(const QString& error);
    void statusChanged(const QString& status);
    void connected();
    void disconnected();
    void reconnected();

private slots:
    void onWriterDataWritten(int bytesWritten);
    void onWriterWriteCompleted();
    void onWriterQueueEmpty();
    void onWriterError(const QString& error);
    void onWriterStatusChanged(const QString& status);
    void attemptReconnect();

private:
    void updateStatus(const QString& status);
    void handleError(const QString& error);
    bool processQueue();

    scpFTDIWriter* m_writer;
    QString m_devicePath;
    
    // Queue management
    QQueue<QByteArray> m_writeQueue;
    mutable QMutex m_queueMutex;
    int m_maxQueueSize;
    bool m_flowControlEnabled;
    
    // Auto-reconnect
    bool m_autoReconnect;
    int m_reconnectDelayMs;
    QTimer* m_reconnectTimer;
    
    // Statistics
    qint64 m_totalBytesWritten;
    int m_errorCount;
    int m_reconnectCount;
    int m_droppedPackets;
    bool m_isConnected;
};

