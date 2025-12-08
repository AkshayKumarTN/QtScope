#pragma once
#include <QObject>
#include <QTimer>
#include <QQueue>
#include "scpFTDIInterface.h"

/**
 * @brief High-level controller for USB read operations
 * 
 * Wraps scpFTDIReader with additional features:
 * - Auto-reconnect on errors
 * - Buffer management
 * - Error recovery
 * - Connection state monitoring
 */
class scpUsbReadController : public QObject {
    Q_OBJECT

public:
    explicit scpUsbReadController(QObject* parent = nullptr);
    ~scpUsbReadController() override;

    // Configuration
    void setDevicePath(const QString& path);
    QString devicePath() const { return m_devicePath; }

    void setSamplingFrequency(double frequencyHz);
    double samplingFrequency() const;

    void setBytesPerRead(int bytes);
    int bytesPerRead() const;

    // Auto-reconnect settings
    void setAutoReconnect(bool enable) { m_autoReconnect = enable; }
    bool autoReconnect() const { return m_autoReconnect; }
    void setReconnectDelay(int ms) { m_reconnectDelayMs = ms; }
    int reconnectDelay() const { return m_reconnectDelayMs; }

    // State
    bool isOpen() const;
    bool isRunning() const;

    // Operations
    bool open();
    void close();
    void start();
    void stop();

    // Statistics
    qint64 totalBytesRead() const { return m_totalBytesRead; }
    int errorCount() const { return m_errorCount; }
    int reconnectCount() const { return m_reconnectCount; }

signals:
    void dataReceived(const QByteArray& data);
    void readCompleted(int bytesRead);
    void errorOccurred(const QString& error);
    void statusChanged(const QString& status);
    void connected();
    void disconnected();
    void reconnected();

private slots:
    void onReaderDataReceived(const QByteArray& data);
    void onReaderReadCompleted(int bytesRead);
    void onReaderError(const QString& error);
    void onReaderStatusChanged(const QString& status);
    void attemptReconnect();

private:
    void updateStatus(const QString& status);
    void handleError(const QString& error);

    scpFTDIReader* m_reader;
    QString m_devicePath;
    bool m_autoReconnect;
    int m_reconnectDelayMs;
    QTimer* m_reconnectTimer;
    
    // Statistics
    qint64 m_totalBytesRead;
    int m_errorCount;
    int m_reconnectCount;
    bool m_isConnected;
};

