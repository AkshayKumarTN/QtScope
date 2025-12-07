#ifndef SCPFTDIINTERFACE_H
#define SCPFTDIINTERFACE_H

#include <QObject>
#include <QTimer>
#include <QFile>
#include <QByteArray>
#include <QString>

/**
 * @brief Base class for FTDI 245R interface operations
 * 
 * This class provides the foundation for reading and writing data
 * via FTDI USB interface (simulated with file I/O for testing)
 */
class scpFTDIInterface : public QObject {
    Q_OBJECT

public:
    explicit scpFTDIInterface(QObject *parent = nullptr);
    virtual ~scpFTDIInterface();

    // Configuration methods
    void setDevicePath(const QString& path);
    QString devicePath() const { return m_devicePath; }
    
    bool isOpen() const { return m_isOpen; }
    bool isRunning() const { return m_isRunning; }

    // Open/Close device
    virtual bool open() = 0;
    virtual void close() = 0;

signals:
    void errorOccurred(const QString& error);
    void statusChanged(const QString& status);

protected:
    QString m_devicePath;
    bool m_isOpen;
    bool m_isRunning;
    QFile m_file;
};

/**
 * @brief FTDI Reader class - implements reqfRead
 * 
 * Continuously reads data bytes from USB port at user-defined
 * sampling frequency and specified number of bytes per read
 */
class scpFTDIReader : public scpFTDIInterface {
    Q_OBJECT

public:
    explicit scpFTDIReader(QObject *parent = nullptr);
    ~scpFTDIReader() override;

    // Configuration
    void setSamplingFrequency(double frequencyHz);
    void setBytesPerRead(int bytes);
    
    double samplingFrequency() const { return m_samplingFrequency; }
    int bytesPerRead() const { return m_bytesPerRead; }

    // Operations
    bool open() override;
    void close() override;
    void start();
    void stop();

signals:
    void dataReceived(const QByteArray& data);
    void readCompleted(int bytesRead);

private slots:
    void performRead();

private:
    QTimer* m_readTimer;
    double m_samplingFrequency;  // Hz
    int m_bytesPerRead;
    qint64 m_totalBytesRead;
};

/**
 * @brief FTDI Writer class - implements reqfWrite
 * 
 * Transmits data bytes to USB port at user-defined output
 * frequency and specified number of bytes per write
 */
class scpFTDIWriter : public scpFTDIInterface {
    Q_OBJECT

public:
    explicit scpFTDIWriter(QObject *parent = nullptr);
    ~scpFTDIWriter() override;

    // Configuration
    void setOutputFrequency(double frequencyHz);
    void setBytesPerWrite(int bytes);
    
    double outputFrequency() const { return m_outputFrequency; }
    int bytesPerWrite() const { return m_bytesPerWrite; }

    // Operations
    bool open() override;
    void close() override;
    void start();
    void stop();
    
    // Queue data for writing
    void queueData(const QByteArray& data);
    int queuedDataSize() const;

signals:
    void dataWritten(int bytesWritten);
    void writeCompleted();
    void queueEmpty();

private slots:
    void performWrite();

private:
    QTimer* m_writeTimer;
    double m_outputFrequency;  // Hz
    int m_bytesPerWrite;
    QByteArray m_writeQueue;
    qint64 m_totalBytesWritten;
};

#endif // SCPFTDIINTERFACE_H