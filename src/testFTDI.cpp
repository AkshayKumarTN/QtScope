#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include "scpFTDIInterface.h"

/**
 * @brief Test application for FTDI Reader and Writer
 * 
 * This test simulates two FTDI devices:
 * - Reader: Reads from input file at specified sampling rate
 * - Writer: Writes to output file at specified output rate
 */
class FTDITest : public QObject {
    Q_OBJECT

public:
    FTDITest(QObject* parent = nullptr) : QObject(parent) {
        // Create reader and writer
        m_reader = new scpFTDIReader(this);
        m_writer = new scpFTDIWriter(this);
        
        // Connect signals for monitoring
        connectSignals();
    }
    
    void runTest(const QString& inputFile, const QString& outputFile,
                 double readFreq, int readBytes,
                 double writeFreq, int writeBytes) {
        
        qDebug() << "=== FTDI Interface Test ===";
        qDebug() << "Input file:" << inputFile;
        qDebug() << "Output file:" << outputFile;
        qDebug() << "Read frequency:" << readFreq << "Hz, Bytes/read:" << readBytes;
        qDebug() << "Write frequency:" << writeFreq << "Hz, Bytes/write:" << writeBytes;
        qDebug() << "";
        
        // Configure reader (reqfRead)
        m_reader->setDevicePath(inputFile);
        m_reader->setSamplingFrequency(readFreq);
        m_reader->setBytesPerRead(readBytes);
        
        // Configure writer (reqfWrite)
        m_writer->setDevicePath(outputFile);
        m_writer->setOutputFrequency(writeFreq);
        m_writer->setBytesPerWrite(writeBytes);
        
        // Open devices
        if (!m_reader->open()) {
            qDebug() << "Failed to open reader!";
            QCoreApplication::quit();
            return;
        }
        
        if (!m_writer->open()) {
            qDebug() << "Failed to open writer!";
            QCoreApplication::quit();
            return;
        }
        
        // Start operations
        m_reader->start();
        m_writer->start();
        
        qDebug() << "Test started. Press Ctrl+C to stop.";
    }
    
private slots:
    void onReaderData(const QByteArray& data) {
        // Forward data from reader to writer
        m_writer->queueData(data);
        
        // Optional: Print first few bytes for verification
        static int count = 0;
        if (count++ < 5) {
            QString hex;
            for (int i = 0; i < qMin(16, data.size()); ++i) {
                hex += QString("%1 ").arg((unsigned char)data[i], 2, 16, QChar('0'));
            }
            qDebug() << "Read" << data.size() << "bytes:" << hex << "...";
        }
    }
    
    void onReaderCompleted(int bytes) {
        m_totalRead += bytes;
    }
    
    void onWriterWritten(int bytes) {
        m_totalWritten += bytes;
    }
    
    void onWriterCompleted() {
        static int count = 0;
        if (count++ % 10 == 0) {
            qDebug() << "Progress - Read:" << m_totalRead 
                     << "bytes, Written:" << m_totalWritten << "bytes";
        }
    }
    
    void onReaderStatus(const QString& status) {
        qDebug() << "[READER]" << status;
        
        // If reader stopped, schedule cleanup
        if (status.contains("stopped") || status.contains("End of")) {
            QTimer::singleShot(2000, this, &FTDITest::cleanup);
        }
    }
    
    void onWriterStatus(const QString& status) {
        qDebug() << "[WRITER]" << status;
    }
    
    void onReaderError(const QString& error) {
        qDebug() << "[READER ERROR]" << error;
    }
    
    void onWriterError(const QString& error) {
        qDebug() << "[WRITER ERROR]" << error;
    }
    
    void cleanup() {
        qDebug() << "\n=== Test Complete ===";
        qDebug() << "Total bytes read:" << m_totalRead;
        qDebug() << "Total bytes written:" << m_totalWritten;
        
        m_reader->stop();
        m_writer->stop();
        m_reader->close();
        m_writer->close();
        
        QTimer::singleShot(500, QCoreApplication::instance(), &QCoreApplication::quit);
    }
    
private:
    void connectSignals() {
        // Reader signals
        connect(m_reader, &scpFTDIReader::dataReceived, 
                this, &FTDITest::onReaderData);
        connect(m_reader, &scpFTDIReader::readCompleted, 
                this, &FTDITest::onReaderCompleted);
        connect(m_reader, &scpFTDIReader::statusChanged, 
                this, &FTDITest::onReaderStatus);
        connect(m_reader, &scpFTDIReader::errorOccurred, 
                this, &FTDITest::onReaderError);
        
        // Writer signals
        connect(m_writer, &scpFTDIWriter::dataWritten, 
                this, &FTDITest::onWriterWritten);
        connect(m_writer, &scpFTDIWriter::writeCompleted, 
                this, &FTDITest::onWriterCompleted);
        connect(m_writer, &scpFTDIWriter::statusChanged, 
                this, &FTDITest::onWriterStatus);
        connect(m_writer, &scpFTDIWriter::errorOccurred, 
                this, &FTDITest::onWriterError);
    }
    
    scpFTDIReader* m_reader;
    scpFTDIWriter* m_writer;
    qint64 m_totalRead = 0;
    qint64 m_totalWritten = 0;
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    // Parse command line arguments
    QString inputFile = "input_data.bin";
    QString outputFile = "output_data.bin";
    double readFreq = 1000.0;   // 1 kHz
    int readBytes = 256;
    double writeFreq = 500.0;   // 500 Hz
    int writeBytes = 128;
    
    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--input" && i + 1 < args.size()) {
            inputFile = args[++i];
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            outputFile = args[++i];
        } else if (args[i] == "--read-freq" && i + 1 < args.size()) {
            readFreq = args[++i].toDouble();
        } else if (args[i] == "--read-bytes" && i + 1 < args.size()) {
            readBytes = args[++i].toInt();
        } else if (args[i] == "--write-freq" && i + 1 < args.size()) {
            writeFreq = args[++i].toDouble();
        } else if (args[i] == "--write-bytes" && i + 1 < args.size()) {
            writeBytes = args[++i].toInt();
        } else if (args[i] == "--help" || args[i] == "-h") {
            qDebug() << "Usage:" << args[0] << "[options]";
            qDebug() << "Options:";
            qDebug() << "  --input <file>       Input file (default: input_data.bin)";
            qDebug() << "  --output <file>      Output file (default: output_data.bin)";
            qDebug() << "  --read-freq <hz>     Read frequency in Hz (default: 1000)";
            qDebug() << "  --read-bytes <n>     Bytes per read (default: 256)";
            qDebug() << "  --write-freq <hz>    Write frequency in Hz (default: 500)";
            qDebug() << "  --write-bytes <n>    Bytes per write (default: 128)";
            return 0;
        }
    }
    
    // Create and run test
    FTDITest test;
    QTimer::singleShot(0, [&]() {
        test.runTest(inputFile, outputFile, readFreq, readBytes, writeFreq, writeBytes);
    });
    
    return app.exec();
}

#include "testFTDI.moc"