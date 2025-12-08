#include "scpUsbWriteController.h"
#include "scpFTDIInterface.h"
#include <QDebug>
#include <QMutexLocker>

scpUsbWriteController::scpUsbWriteController(QObject* parent)
    : QObject(parent)
    , m_writer(new scpFTDIWriter(this))
    , m_maxQueueSize(1000)  // Default: 1000 packets
    , m_flowControlEnabled(true)
    , m_autoReconnect(true)
    , m_reconnectDelayMs(1000)
    , m_reconnectTimer(new QTimer(this))
    , m_totalBytesWritten(0)
    , m_errorCount(0)
    , m_reconnectCount(0)
    , m_droppedPackets(0)
    , m_isConnected(false)
{
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &scpUsbWriteController::attemptReconnect);

    // Forward signals from writer
    connect(m_writer, &scpFTDIWriter::dataWritten,
            this, &scpUsbWriteController::onWriterDataWritten);
    connect(m_writer, &scpFTDIWriter::writeCompleted,
            this, &scpUsbWriteController::onWriterWriteCompleted);
    connect(m_writer, &scpFTDIWriter::queueEmpty,
            this, &scpUsbWriteController::onWriterQueueEmpty);
    connect(m_writer, &scpFTDIWriter::errorOccurred,
            this, &scpUsbWriteController::onWriterError);
    connect(m_writer, &scpFTDIWriter::statusChanged,
            this, &scpUsbWriteController::onWriterStatusChanged);
}

scpUsbWriteController::~scpUsbWriteController() {
    close();
}

void scpUsbWriteController::setDevicePath(const QString& path) {
    m_devicePath = path;
    m_writer->setDevicePath(path);
}

void scpUsbWriteController::setOutputFrequency(double frequencyHz) {
    m_writer->setOutputFrequency(frequencyHz);
}

double scpUsbWriteController::outputFrequency() const {
    return m_writer->outputFrequency();
}

void scpUsbWriteController::setBytesPerWrite(int bytes) {
    m_writer->setBytesPerWrite(bytes);
}

int scpUsbWriteController::bytesPerWrite() const {
    return m_writer->bytesPerWrite();
}

int scpUsbWriteController::queueSize() const {
    QMutexLocker lock(&m_queueMutex);
    return m_writeQueue.size();
}

bool scpUsbWriteController::isOpen() const {
    return m_writer && m_writer->isOpen();
}

bool scpUsbWriteController::isRunning() const {
    return m_writer && m_writer->isRunning();
}

bool scpUsbWriteController::open() {
    if (m_writer->open()) {
        m_isConnected = true;
        m_errorCount = 0;  // Reset error count on successful open
        emit connected();
        updateStatus("USB write controller opened");
        return true;
    } else {
        handleError("Failed to open USB write controller");
        return false;
    }
}

void scpUsbWriteController::close() {
    m_reconnectTimer->stop();
    clearQueue();
    if (m_writer) {
        m_writer->stop();
        m_writer->close();
    }
    if (m_isConnected) {
        m_isConnected = false;
        emit disconnected();
    }
    updateStatus("USB write controller closed");
}

void scpUsbWriteController::start() {
    if (!m_writer->isOpen()) {
        if (!open()) {
            if (m_autoReconnect) {
                attemptReconnect();
            }
            return;
        }
    }

    m_writer->start();
    
    // Process any queued data
    processQueue();
    
    updateStatus("USB write controller started");
}

void scpUsbWriteController::stop() {
    m_reconnectTimer->stop();
    if (m_writer) {
        m_writer->stop();
    }
    updateStatus("USB write controller stopped");
}

bool scpUsbWriteController::queueData(const QByteArray& data) {
    QMutexLocker lock(&m_queueMutex);
    
    // Check queue size
    if (m_writeQueue.size() >= m_maxQueueSize) {
        if (m_flowControlEnabled) {
            // Drop oldest packet
            m_writeQueue.dequeue();
            m_droppedPackets++;
            emit queueFull();
        } else {
            // Reject new data
            return false;
        }
    }
    
    m_writeQueue.enqueue(data);
    
    // If writer is running, try to send immediately
    if (m_writer && m_writer->isRunning() && m_writer->isOpen()) {
        lock.unlock();
        processQueue();
    }
    
    return true;
}

void scpUsbWriteController::clearQueue() {
    QMutexLocker lock(&m_queueMutex);
    m_writeQueue.clear();
}

bool scpUsbWriteController::processQueue() {
    if (!m_writer || !m_writer->isOpen() || !m_writer->isRunning()) {
        return false;
    }

    QMutexLocker lock(&m_queueMutex);
    
    // Send data from queue to writer
    while (!m_writeQueue.isEmpty() && m_writer->queuedDataSize() < m_maxQueueSize / 2) {
        QByteArray data = m_writeQueue.dequeue();
        m_writer->queueData(data);
    }

    // Check if queue is empty
    if (m_writeQueue.isEmpty()) {
        lock.unlock();
        emit queueEmpty();
    }

    return true;
}

void scpUsbWriteController::onWriterDataWritten(int bytesWritten) {
    m_totalBytesWritten += bytesWritten;
    emit dataWritten(bytesWritten);
    
    // Process more from queue
    processQueue();
}

void scpUsbWriteController::onWriterWriteCompleted() {
    emit writeCompleted();
    processQueue();
}

void scpUsbWriteController::onWriterQueueEmpty() {
    emit queueEmpty();
    processQueue();
}

void scpUsbWriteController::onWriterError(const QString& error) {
    m_errorCount++;
    handleError(error);
    
    // Auto-reconnect on error if enabled
    if (m_autoReconnect && m_writer->isRunning()) {
        m_writer->stop();
        m_isConnected = false;
        emit disconnected();
        attemptReconnect();
    }
}

void scpUsbWriteController::onWriterStatusChanged(const QString& status) {
    updateStatus(status);
}

void scpUsbWriteController::attemptReconnect() {
    if (!m_autoReconnect) return;
    
    updateStatus(QString("Attempting to reconnect... (delay: %1 ms)").arg(m_reconnectDelayMs));
    
    if (open()) {
        m_reconnectCount++;
        emit reconnected();
        
        // Restart if we were running before
        if (m_writer) {
            m_writer->start();
            processQueue();
        }
    } else {
        // Schedule another reconnect attempt
        m_reconnectTimer->start(m_reconnectDelayMs);
    }
}

void scpUsbWriteController::updateStatus(const QString& status) {
    emit statusChanged(QString("[USB Write Controller] %1").arg(status));
}

void scpUsbWriteController::handleError(const QString& error) {
    QString fullError = QString("[USB Write Controller Error] %1").arg(error);
    qWarning() << fullError;
    emit errorOccurred(fullError);
}

