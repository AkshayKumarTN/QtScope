#include "scpFTDIInterface.h"
#include <QDebug>

// ============================================================================
// scpFTDIInterface - Base Class Implementation
// ============================================================================

scpFTDIInterface::scpFTDIInterface(QObject *parent)
    : QObject(parent)
    , m_isOpen(false)
    , m_isRunning(false)
{
}

scpFTDIInterface::~scpFTDIInterface() {
    if (m_file.isOpen()) {
        m_file.close();
    }
}

void scpFTDIInterface::setDevicePath(const QString& path) {
    if (m_isOpen) {
        emit errorOccurred("Cannot change device path while open");
        return;
    }
    m_devicePath = path;
}

// ============================================================================
// scpFTDIReader - Reader Implementation (reqfRead)
// ============================================================================

scpFTDIReader::scpFTDIReader(QObject *parent)
    : scpFTDIInterface(parent)
    , m_readTimer(new QTimer(this))
    , m_samplingFrequency(1000.0)  // Default 1 kHz
    , m_bytesPerRead(256)           // Default 256 bytes
    , m_totalBytesRead(0)
{
    connect(m_readTimer, &QTimer::timeout, this, &scpFTDIReader::performRead);
}

scpFTDIReader::~scpFTDIReader() {
    stop();
    close();
}

void scpFTDIReader::setSamplingFrequency(double frequencyHz) {
    if (frequencyHz <= 0) {
        emit errorOccurred("Sampling frequency must be positive");
        return;
    }
    
    bool wasRunning = m_isRunning;
    if (wasRunning) stop();
    
    m_samplingFrequency = frequencyHz;
    
    if (wasRunning) start();
    
    emit statusChanged(QString("Sampling frequency set to %1 Hz").arg(frequencyHz));
}

void scpFTDIReader::setBytesPerRead(int bytes) {
    if (bytes <= 0) {
        emit errorOccurred("Bytes per read must be positive");
        return;
    }
    m_bytesPerRead = bytes;
    emit statusChanged(QString("Bytes per read set to %1").arg(bytes));
}

bool scpFTDIReader::open() {
    if (m_isOpen) {
        emit errorOccurred("Device already open");
        return false;
    }
    
    if (m_devicePath.isEmpty()) {
        emit errorOccurred("Device path not set");
        return false;
    }
    
    m_file.setFileName(m_devicePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(QString("Failed to open device: %1").arg(m_file.errorString()));
        return false;
    }
    
    m_isOpen = true;
    m_totalBytesRead = 0;
    emit statusChanged(QString("Reader opened: %1").arg(m_devicePath));
    return true;
}

void scpFTDIReader::close() {
    stop();
    
    if (m_file.isOpen()) {
        m_file.close();
    }
    
    m_isOpen = false;
    emit statusChanged("Reader closed");
}

void scpFTDIReader::start() {
    if (!m_isOpen) {
        emit errorOccurred("Cannot start: device not open");
        return;
    }
    
    if (m_isRunning) {
        return;
    }
    
    // Calculate timer interval from sampling frequency
    int intervalMs = static_cast<int>(1000.0 / m_samplingFrequency);
    if (intervalMs < 1) intervalMs = 1;
    
    m_readTimer->start(intervalMs);
    m_isRunning = true;
    emit statusChanged(QString("Reader started: %1 Hz (%2 ms interval)")
                       .arg(m_samplingFrequency).arg(intervalMs));
}

void scpFTDIReader::stop() {
    if (!m_isRunning) {
        return;
    }
    
    m_readTimer->stop();
    m_isRunning = false;
    emit statusChanged(QString("Reader stopped. Total bytes read: %1").arg(m_totalBytesRead));
}

void scpFTDIReader::performRead() {
    if (!m_isOpen || !m_file.isOpen()) {
        emit errorOccurred("Device not open");
        stop();
        return;
    }
    
    // Read specified number of bytes
    QByteArray data = m_file.read(m_bytesPerRead);
    
    if (data.isEmpty() && m_file.atEnd()) {
        // End of file reached
        emit statusChanged("End of input file reached");
        stop();
        return;
    }
    
    if (data.isEmpty() && m_file.error() != QFile::NoError) {
        emit errorOccurred(QString("Read error: %1").arg(m_file.errorString()));
        stop();
        return;
    }
    
    m_totalBytesRead += data.size();
    
    // Emit the data
    emit dataReceived(data);
    emit readCompleted(data.size());
}

// ============================================================================
// scpFTDIWriter - Writer Implementation (reqfWrite)
// ============================================================================

scpFTDIWriter::scpFTDIWriter(QObject *parent)
    : scpFTDIInterface(parent)
    , m_writeTimer(new QTimer(this))
    , m_outputFrequency(1000.0)  // Default 1 kHz
    , m_bytesPerWrite(256)        // Default 256 bytes
    , m_totalBytesWritten(0)
{
    connect(m_writeTimer, &QTimer::timeout, this, &scpFTDIWriter::performWrite);
}

scpFTDIWriter::~scpFTDIWriter() {
    stop();
    close();
}

void scpFTDIWriter::setOutputFrequency(double frequencyHz) {
    if (frequencyHz <= 0) {
        emit errorOccurred("Output frequency must be positive");
        return;
    }
    
    bool wasRunning = m_isRunning;
    if (wasRunning) stop();
    
    m_outputFrequency = frequencyHz;
    
    if (wasRunning) start();
    
    emit statusChanged(QString("Output frequency set to %1 Hz").arg(frequencyHz));
}

void scpFTDIWriter::setBytesPerWrite(int bytes) {
    if (bytes <= 0) {
        emit errorOccurred("Bytes per write must be positive");
        return;
    }
    m_bytesPerWrite = bytes;
    emit statusChanged(QString("Bytes per write set to %1").arg(bytes));
}

bool scpFTDIWriter::open() {
    if (m_isOpen) {
        emit errorOccurred("Device already open");
        return false;
    }
    
    if (m_devicePath.isEmpty()) {
        emit errorOccurred("Device path not set");
        return false;
    }
    
    m_file.setFileName(m_devicePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred(QString("Failed to open device: %1").arg(m_file.errorString()));
        return false;
    }
    
    m_isOpen = true;
    m_totalBytesWritten = 0;
    m_writeQueue.clear();
    emit statusChanged(QString("Writer opened: %1").arg(m_devicePath));
    return true;
}

void scpFTDIWriter::close() {
    stop();
    
    if (m_file.isOpen()) {
        // Flush any remaining data
        if (!m_writeQueue.isEmpty()) {
            m_file.write(m_writeQueue);
            m_file.flush();
        }
        m_file.close();
    }
    
    m_isOpen = false;
    emit statusChanged(QString("Writer closed. Total bytes written: %1").arg(m_totalBytesWritten));
}

void scpFTDIWriter::start() {
    if (!m_isOpen) {
        emit errorOccurred("Cannot start: device not open");
        return;
    }
    
    if (m_isRunning) {
        return;
    }
    
    // Calculate timer interval from output frequency
    int intervalMs = static_cast<int>(1000.0 / m_outputFrequency);
    if (intervalMs < 1) intervalMs = 1;
    
    m_writeTimer->start(intervalMs);
    m_isRunning = true;
    emit statusChanged(QString("Writer started: %1 Hz (%2 ms interval)")
                       .arg(m_outputFrequency).arg(intervalMs));
}

void scpFTDIWriter::stop() {
    if (!m_isRunning) {
        return;
    }
    
    m_writeTimer->stop();
    m_isRunning = false;
    emit statusChanged(QString("Writer stopped. Queue size: %1, Total written: %2")
                       .arg(m_writeQueue.size()).arg(m_totalBytesWritten));
}

void scpFTDIWriter::queueData(const QByteArray& data) {
    m_writeQueue.append(data);
}

int scpFTDIWriter::queuedDataSize() const {
    return m_writeQueue.size();
}

void scpFTDIWriter::performWrite() {
    if (!m_isOpen || !m_file.isOpen()) {
        emit errorOccurred("Device not open");
        stop();
        return;
    }
    
    if (m_writeQueue.isEmpty()) {
        emit queueEmpty();
        return;
    }
    
    // Write specified number of bytes
    int bytesToWrite = qMin(m_bytesPerWrite, m_writeQueue.size());
    QByteArray data = m_writeQueue.left(bytesToWrite);
    
    qint64 written = m_file.write(data);
    
    if (written < 0) {
        emit errorOccurred(QString("Write error: %1").arg(m_file.errorString()));
        stop();
        return;
    }
    
    // Remove written data from queue
    m_writeQueue.remove(0, written);
    m_totalBytesWritten += written;
    
    emit dataWritten(written);
    
    if (m_writeQueue.isEmpty()) {
        emit writeCompleted();
        emit queueEmpty();
    }
}