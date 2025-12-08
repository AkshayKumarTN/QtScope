#include "scpUsbReadController.h"
#include "scpFTDIInterface.h"
#include <QDebug>

scpUsbReadController::scpUsbReadController(QObject* parent)
    : QObject(parent)
    , m_reader(new scpFTDIReader(this))
    , m_autoReconnect(true)
    , m_reconnectDelayMs(1000)
    , m_reconnectTimer(new QTimer(this))
    , m_totalBytesRead(0)
    , m_errorCount(0)
    , m_reconnectCount(0)
    , m_isConnected(false)
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &scpUsbReadController::attemptReconnect);

    // Forward signals from reader
    connect(m_reader, &scpFTDIReader::dataReceived,
            this, &scpUsbReadController::onReaderDataReceived);
    connect(m_reader, &scpFTDIReader::readCompleted,
            this, &scpUsbReadController::onReaderReadCompleted);
    connect(m_reader, &scpFTDIReader::errorOccurred,
            this, &scpUsbReadController::onReaderError);
    connect(m_reader, &scpFTDIReader::statusChanged,
            this, &scpUsbReadController::onReaderStatusChanged);
}

scpUsbReadController::~scpUsbReadController() {
    close();
}

void scpUsbReadController::setDevicePath(const QString& path) {
    m_devicePath = path;
    m_reader->setDevicePath(path);
}

void scpUsbReadController::setSamplingFrequency(double frequencyHz) {
    m_reader->setSamplingFrequency(frequencyHz);
}

double scpUsbReadController::samplingFrequency() const {
    return m_reader->samplingFrequency();
}

void scpUsbReadController::setBytesPerRead(int bytes) {
    m_reader->setBytesPerRead(bytes);
}

int scpUsbReadController::bytesPerRead() const {
    return m_reader->bytesPerRead();
}

bool scpUsbReadController::isOpen() const {
    return m_reader && m_reader->isOpen();
}

bool scpUsbReadController::isRunning() const {
    return m_reader && m_reader->isRunning();
}

bool scpUsbReadController::open() {
    if (m_reader->open()) {
        m_isConnected = true;
        m_errorCount = 0;  // Reset error count on successful open
        emit connected();
        updateStatus("USB read controller opened");
        return true;
    } else {
        handleError("Failed to open USB read controller");
        return false;
    }
}

void scpUsbReadController::close() {
    m_reconnectTimer->stop();
    if (m_reader) {
        m_reader->stop();
        m_reader->close();
    }
    if (m_isConnected) {
        m_isConnected = false;
        emit disconnected();
    }
    updateStatus("USB read controller closed");
}

void scpUsbReadController::start() {
    if (!m_reader->isOpen()) {
        if (!open()) {
            if (m_autoReconnect) {
                attemptReconnect();
            }
            return;
        }
    }

    m_reader->start();
    updateStatus("USB read controller started");
}

void scpUsbReadController::stop() {
    m_reconnectTimer->stop();
    if (m_reader) {
        m_reader->stop();
    }
    updateStatus("USB read controller stopped");
}

void scpUsbReadController::onReaderDataReceived(const QByteArray& data) {
    m_totalBytesRead += data.size();
    emit dataReceived(data);
}

void scpUsbReadController::onReaderReadCompleted(int bytesRead) {
    emit readCompleted(bytesRead);
}

void scpUsbReadController::onReaderError(const QString& error) {
    m_errorCount++;
    handleError(error);
    
    // Auto-reconnect on error if enabled
    if (m_autoReconnect && m_reader->isRunning()) {
        m_reader->stop();
        m_isConnected = false;
        emit disconnected();
        attemptReconnect();
    }
}

void scpUsbReadController::onReaderStatusChanged(const QString& status) {
    updateStatus(status);
}

void scpUsbReadController::attemptReconnect() {
    if (!m_autoReconnect) return;
    
    updateStatus(QString("Attempting to reconnect... (delay: %1 ms)").arg(m_reconnectDelayMs));
    
    if (open()) {
        m_reconnectCount++;
        emit reconnected();
        
        // Restart if we were running before
        if (m_reader) {
            m_reader->start();
        }
    } else {
        // Schedule another reconnect attempt
        m_reconnectTimer->start(m_reconnectDelayMs);
    }
}

void scpUsbReadController::updateStatus(const QString& status) {
    emit statusChanged(QString("[USB Read Controller] %1").arg(status));
}

void scpUsbReadController::handleError(const QString& error) {
    QString fullError = QString("[USB Read Controller Error] %1").arg(error);
    qWarning() << fullError;
    emit errorOccurred(fullError);
}

