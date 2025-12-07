#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <cmath>

/**
 * @brief Generate test data file for FTDI testing
 * 
 * Creates a binary file with sample waveform data
 */
bool generateTestData(const QString& filename, int numBytes) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to create file:" << filename;
        return false;
    }
    
    qDebug() << "Generating" << numBytes << "bytes of test data...";
    
    // Generate sine wave + sawtooth pattern
    for (int i = 0; i < numBytes; ++i) {
        // Combine multiple frequencies for interesting pattern
        double t = i / 100.0;
        double sine = 127.0 * sin(2.0 * M_PI * t);
        double sawtooth = 50.0 * (fmod(t * 5.0, 1.0) - 0.5);
        
        unsigned char value = static_cast<unsigned char>(128 + sine + sawtooth);
        file.write(reinterpret_cast<const char*>(&value), 1);
    }
    
    file.close();
    qDebug() << "Test data written to:" << filename;
    return true;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    QString filename = "input_data.bin";
    int numBytes = 100000;  // 100 KB default
    
    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--output" && i + 1 < args.size()) {
            filename = args[++i];
        } else if (args[i] == "--size" && i + 1 < args.size()) {
            numBytes = args[++i].toInt();
        } else if (args[i] == "--help" || args[i] == "-h") {
            qDebug() << "Usage:" << args[0] << "[options]";
            qDebug() << "Options:";
            qDebug() << "  --output <file>  Output file (default: input_data.bin)";
            qDebug() << "  --size <bytes>   Number of bytes (default: 100000)";
            return 0;
        }
    }
    
    if (generateTestData(filename, numBytes)) {
        return 0;
    } else {
        return 1;
    }
}