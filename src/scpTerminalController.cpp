#include "scpTerminalController.h"
#include "scpView.h"
#include "scpSignalGeneratorSource.h"
#include "scpSimulatedGeneratorSource.h"
#include "scpSimulatedAcquisitionSource.h"
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>

scpTerminalController::scpTerminalController(QObject* parent)
    : QObject(parent) {
    m_sampleForTimer = new QTimer(this);
    m_sampleForTimer->setSingleShot(true);
    connect(m_sampleForTimer, &QTimer::timeout, this, &scpTerminalController::onSampleForTimeout);
}

scpTerminalController::~scpTerminalController() {
    // Cleanup handled by Qt parent-child relationship
}

void scpTerminalController::setSource(scpDataSource* source) {
    m_source = source;
}

void scpTerminalController::setAcquisitionSource(scpDataSource* source) {
    m_acquisitionSource = source;
}

void scpTerminalController::setGeneratorSource(scpDataSource* source) {
    m_generatorSource = source;
}

void scpTerminalController::setView(scpView* view) {
    m_view = view;
}

void scpTerminalController::setOutputStream(QTextStream* stream) {
    m_out = stream;
}

bool scpTerminalController::processCommand(const QString& line) {
    if (!m_out) {
        qWarning() << "scpTerminalController: No output stream set";
        return false;
    }

    // Support both "scope start" and just "start"
    QString processedLine = line.trimmed().toLower();
    if (processedLine.startsWith("scope ")) {
        processedLine = processedLine.mid(6);  // Remove "scope " prefix
    }

    const QString cmd = processedLine.section(' ', 0, 0);
    const QString arg = processedLine.section(' ', 1);

    if (cmd == "quit" || cmd == "exit") {
        emit quitRequested();
        return true;
    } else if (cmd == "start") {
        return handleStart(arg);
    } else if (cmd == "stop") {
        return handleStop(arg);
    } else if (cmd == "combined") {
        return handleCombined(arg);
    } else if (cmd.startsWith("sampletime=")) {
        return handleSampleTime(cmd.mid(11));  // Remove "sampletime="
    } else if (cmd.startsWith("samplefor=")) {
        return handleSampleFor(cmd.mid(10));  // Remove "samplefor="
    } else if (cmd == "timebase_ms") {
        return handleTimebase(arg);
    } else if (cmd == "scale") {
        return handleScale(arg);
    } else if (cmd == "freq") {
        return handleFrequency(arg);
    } else if (cmd == "amplitude") {
        return handleAmplitude(arg);
    } else if (cmd == "offset") {
        return handleOffset(arg);
    } else if (cmd == "waveform") {
        return handleWaveform(arg);
    } else if (cmd == "noiselevel") {
        return handleNoiseLevel(arg);
    } else if (cmd == "status") {
        return handleStatus(arg);
    } else if (cmd == "help" || cmd == "?") {
        return handleHelp(arg);
    } else if (cmd.isEmpty()) {
        return true;  // Empty line, just continue
    } else {
        writeResponse(QString("Unknown command: %1. Type 'help' for commands.").arg(cmd));
        return false;
    }
}

bool scpTerminalController::handleStart(const QString& arg) {
    Q_UNUSED(arg);
    if (m_source) {
        if (!m_source->start()) {
            writeResponse("✗ Failed to start acquisition.");
            return false;
        } else {
            writeResponse("✓ Acquisition started. Waveform displaying...");
            emit startRequested();
            return true;
        }
    } else {
        writeResponse("✗ No source configured.");
        return false;
    }
}

bool scpTerminalController::handleStop(const QString& arg) {
    Q_UNUSED(arg);
    if (m_combinedMode) {
        // Stop both sources in combined mode
        if (m_acquisitionSource && m_acquisitionSource->isActive()) {
            m_acquisitionSource->stop();
        }
        if (m_generatorSource && m_generatorSource->isActive()) {
            m_generatorSource->stop();
        }
        m_combinedMode = false;
        writeResponse("Combined mode stopped.");
    } else {
        // Stop single source
        if (m_source && m_source->isActive()) {
            m_source->stop();
        }
        writeResponse("Acquisition stopped.");
    }
    if (m_sampleForTimer) {
        m_sampleForTimer->stop();
    }
    emit stopRequested();
    return true;
}

bool scpTerminalController::handleCombined(const QString& arg) {
    Q_UNUSED(arg);
    
    // Check if sources are available
    if (!m_acquisitionSource || !m_generatorSource) {
        writeResponse("[Error] Combined mode requires both acquisition and generator sources to be configured.");
        return false;
    }
    
    // Check if already running
    if ((m_acquisitionSource && m_acquisitionSource->isActive()) ||
        (m_generatorSource && m_generatorSource->isActive())) {
        writeResponse("[Error] Acquisition or generator already running. Stop first.");
        return false;
    }
    
    // Start both sources
    bool acqStarted = m_acquisitionSource->start();
    bool genStarted = m_generatorSource->start();
    
    if (acqStarted && genStarted) {
        m_combinedMode = true;
        writeResponse("[OK] Combined mode started. Both acquisition and generation running.");
        emit startRequested();
        return true;
    } else {
        // If one failed, stop the other
        if (acqStarted) m_acquisitionSource->stop();
        if (genStarted) m_generatorSource->stop();
        writeResponse("[Error] Failed to start combined mode.");
        return false;
    }
}

bool scpTerminalController::handleSampleTime(const QString& valueStr) {
    QString unit;
    double value = parseTimeValue(valueStr, unit);
    
    if (value > 0.0 && m_view) {
        double timeWindow = (unit == "s") ? value * 1000.0 : value;  // Convert to ms
        m_view->setTotalTimeWindowSec((timeWindow / 1000.0) * 10.0);  // Convert to 10-div window
        double timePerDiv = timeWindow / 10.0;
        writeResponse(QString("✓ Sample time set to %1%2 (Time/div: %3 ms)")
                     .arg(value).arg(unit).arg(timePerDiv, 0, 'f', 1));
        emit viewUpdateNeeded();
        return true;
    } else {
        writeResponse("Invalid sampleTime value");
        return false;
    }
}

bool scpTerminalController::handleSampleFor(const QString& valueStr) {
    QString unit;
    double value = parseTimeValue(valueStr, unit);
    
    if (value > 0.0) {
        m_sampleForDurationMs = (unit == "s") ? static_cast<int>(value * 1000.0) : static_cast<int>(value);
        
        // Start acquisition if not already running
        if (m_source && !m_source->isActive()) {
            m_source->start();
            emit startRequested();
        }
        
        // Set up timer to stop after duration
        if (m_sampleForTimer) {
            m_sampleForTimer->stop();
        }
        m_sampleForTimer->start(m_sampleForDurationMs);
        writeResponse(QString("Sampling for %1%2 (will auto-stop)").arg(value).arg(unit));
        return true;
    } else {
        writeResponse("Invalid sampleFor value");
        return false;
    }
}

void scpTerminalController::onSampleForTimeout() {
    if (m_source && m_source->isActive()) {
        m_source->stop();
    }
    writeResponse("Sampling duration completed.");
    emit stopRequested();
}

bool scpTerminalController::handleTimebase(const QString& arg) {
    bool ok = false;
    double msdiv = arg.toDouble(&ok);
    if (ok && msdiv > 0.0 && m_view) {
        m_view->setTotalTimeWindowSec((msdiv / 1000.0) * 10.0);
        writeResponse(QString("Timebase set to %1 ms/div").arg(msdiv));
        emit viewUpdateNeeded();
        return true;
    }
    return false;
}

bool scpTerminalController::handleScale(const QString& arg) {
    bool ok = false;
    double u = arg.toDouble(&ok);
    if (ok && u > 0.0 && m_view) {
        m_view->setVerticalScale(static_cast<float>(u));
        writeResponse(QString("Vertical scale set to %1 units/div").arg(u));
        emit viewUpdateNeeded();
        return true;
    }
    return false;
}

bool scpTerminalController::handleFrequency(const QString& arg) {
    bool ok = false;
    double hz = arg.toDouble(&ok);
    if (!ok || hz <= 0.0) return false;
    
    // In combined mode, use generator source; otherwise use main source
    scpDataSource* targetSource = m_combinedMode ? m_generatorSource : m_source;
    if (!targetSource) return false;
    
    auto gen = dynamic_cast<scpSignalGeneratorSource*>(targetSource);
    auto simGen = dynamic_cast<scpSimulatedGeneratorSource*>(targetSource);
    
    if (gen) {
        gen->setFrequency(hz);
        writeResponse(QString("Frequency set to %1 Hz").arg(hz));
        return true;
    } else if (simGen) {
        simGen->setFrequency(hz);
        writeResponse(QString("Frequency set to %1 Hz").arg(hz));
        return true;
    } else {
        writeResponse("Frequency control not available for this source");
        return false;
    }
}

bool scpTerminalController::handleAmplitude(const QString& arg) {
    bool ok = false;
    double amp = arg.toDouble(&ok);
    if (!ok || amp <= 0.0) return false;
    
    // In combined mode, use generator source; otherwise use main source
    scpDataSource* targetSource = m_combinedMode ? m_generatorSource : m_source;
    if (!targetSource) return false;
    
    auto simGen = dynamic_cast<scpSimulatedGeneratorSource*>(targetSource);
    if (simGen) {
        simGen->setAmplitude(static_cast<float>(amp));
        writeResponse(QString("Amplitude set to %1").arg(amp));
        return true;
    } else {
        writeResponse("Amplitude control not available for this source");
        return false;
    }
}

bool scpTerminalController::handleOffset(const QString& arg) {
    bool ok = false;
    double offset = arg.toDouble(&ok);
    if (!ok) return false;
    
    // In combined mode, use generator source; otherwise use main source
    scpDataSource* targetSource = m_combinedMode ? m_generatorSource : m_source;
    if (!targetSource) return false;
    
    auto simGen = dynamic_cast<scpSimulatedGeneratorSource*>(targetSource);
    if (simGen) {
        simGen->setOffset(static_cast<float>(offset));
        writeResponse(QString("Offset set to %1").arg(offset));
        return true;
    } else {
        writeResponse("Offset control not available for this source");
        return false;
    }
}

bool scpTerminalController::handleWaveform(const QString& arg) {
    // In combined mode, use generator source; otherwise use main source
    scpDataSource* targetSource = m_combinedMode ? m_generatorSource : m_source;
    if (!targetSource) return false;
    
    auto simGen = dynamic_cast<scpSimulatedGeneratorSource*>(targetSource);
    if (!simGen) {
        writeResponse("Waveform control not available for this source");
        return false;
    }

    QString typeStr = arg.toLower();
    if (typeStr == "sine") {
        simGen->setWaveformType(scpSimulatedGeneratorSource::Sine);
        writeResponse("Waveform set to Sine");
        return true;
    } else if (typeStr == "square") {
        simGen->setWaveformType(scpSimulatedGeneratorSource::Square);
        writeResponse("Waveform set to Square");
        return true;
    } else if (typeStr == "triangle") {
        simGen->setWaveformType(scpSimulatedGeneratorSource::Triangle);
        writeResponse("Waveform set to Triangle");
        return true;
    } else {
        writeResponse("Invalid waveform type. Use: sine, square, or triangle");
        return false;
    }
}

bool scpTerminalController::handleNoiseLevel(const QString& arg) {
    bool ok = false;
    double level = arg.toDouble(&ok);
    if (!ok || level < 0.0 || level > 1.0) {
        writeResponse("Noise level must be between 0.0 and 1.0");
        return false;
    }
    
    // In combined mode, use acquisition source; otherwise use main source
    scpDataSource* targetSource = m_combinedMode ? m_acquisitionSource : m_source;
    if (!targetSource) return false;
    
    auto simAcq = dynamic_cast<scpSimulatedAcquisitionSource*>(targetSource);
    if (simAcq) {
        simAcq->setNoiseLevel(static_cast<float>(level));
        writeResponse(QString("Noise level set to %1").arg(level));
        return true;
    } else {
        writeResponse("Noise level control not available for this source");
        return false;
    }
}

bool scpTerminalController::handleStatus(const QString& arg) {
    Q_UNUSED(arg);
    
    *m_out << Qt::endl << "--- Current Scope Status ---" << Qt::endl;
    
    if (m_combinedMode) {
        *m_out << "  Mode: Combined (Acquisition + Generation)" << Qt::endl;
        if (m_acquisitionSource) {
            *m_out << "  Acquisition Source: " << m_acquisitionSource->metaObject()->className() << Qt::endl;
            *m_out << "  Acquisition Active: " << (m_acquisitionSource->isActive() ? "Yes" : "No") << Qt::endl;
            *m_out << "  Acquisition Sample Rate: " << m_acquisitionSource->sampleRate() << " Hz" << Qt::endl;
            auto simAcq = dynamic_cast<scpSimulatedAcquisitionSource*>(m_acquisitionSource);
            if (simAcq) {
                *m_out << "  Acq Freq: " << simAcq->frequency() << " Hz" << Qt::endl;
                *m_out << "  Acq Noise Level: " << simAcq->noiseLevel() << Qt::endl;
            }
        }
        if (m_generatorSource) {
            *m_out << "  Generator Source: " << m_generatorSource->metaObject()->className() << Qt::endl;
            *m_out << "  Generator Active: " << (m_generatorSource->isActive() ? "Yes" : "No") << Qt::endl;
            *m_out << "  Generator Sample Rate: " << m_generatorSource->sampleRate() << " Hz" << Qt::endl;
            auto simGen = dynamic_cast<scpSimulatedGeneratorSource*>(m_generatorSource);
            if (simGen) {
                *m_out << "  Gen Freq: " << simGen->frequency() << " Hz" << Qt::endl;
                *m_out << "  Gen Amplitude: " << simGen->amplitude() << Qt::endl;
                *m_out << "  Gen Offset: " << simGen->offset() << Qt::endl;
                *m_out << "  Gen Waveform: ";
                switch (simGen->waveformType()) {
                    case scpSimulatedGeneratorSource::Sine: *m_out << "Sine"; break;
                    case scpSimulatedGeneratorSource::Square: *m_out << "Square"; break;
                    case scpSimulatedGeneratorSource::Triangle: *m_out << "Triangle"; break;
                }
                *m_out << Qt::endl;
            }
        }
    } else if (m_source) {
        *m_out << "  Mode: Single Source" << Qt::endl;
        *m_out << "  Source: " << m_source->metaObject()->className() << Qt::endl;
        *m_out << "  Active: " << (m_source->isActive() ? "Yes" : "No") << Qt::endl;
        *m_out << "  Sample Rate: " << m_source->sampleRate() << " Hz" << Qt::endl;

        // Add generator specific info
        auto simGen = dynamic_cast<scpSimulatedGeneratorSource*>(m_source);
        if (simGen) {
            *m_out << "  Gen Freq: " << simGen->frequency() << " Hz" << Qt::endl;
            *m_out << "  Gen Amplitude: " << simGen->amplitude() << Qt::endl;
            *m_out << "  Gen Offset: " << simGen->offset() << Qt::endl;
            *m_out << "  Gen Waveform: ";
            switch (simGen->waveformType()) {
                case scpSimulatedGeneratorSource::Sine: *m_out << "Sine"; break;
                case scpSimulatedGeneratorSource::Square: *m_out << "Square"; break;
                case scpSimulatedGeneratorSource::Triangle: *m_out << "Triangle"; break;
            }
            *m_out << Qt::endl;
        }

        // Add acquisition specific info
        auto simAcq = dynamic_cast<scpSimulatedAcquisitionSource*>(m_source);
        if (simAcq) {
            *m_out << "  Acq Freq: " << simAcq->frequency() << " Hz" << Qt::endl;
            *m_out << "  Acq Noise Level: " << simAcq->noiseLevel() << Qt::endl;
        }
    } else {
        *m_out << "  No source configured" << Qt::endl;
        m_out->flush();
        return false;
    }

    *m_out << "----------------------------" << Qt::endl;
    m_out->flush();
    return true;
}

bool scpTerminalController::handleHelp(const QString& arg) {
    Q_UNUSED(arg);
    *m_out << Qt::endl;
    *m_out << "=== SimpleScope Terminal Commands ===" << Qt::endl;
    *m_out << Qt::endl;
    *m_out << "Control Commands:" << Qt::endl;
    *m_out << "  scope start | start          Start acquisition" << Qt::endl;
    *m_out << "  scope stop | stop            Stop acquisition" << Qt::endl;
    *m_out << "  scope sampleTime=<ms|s>      Set time window (e.g., sampleTime=1ms)" << Qt::endl;
    *m_out << "  scope sampleFor=<ms|s>       Sample for duration (e.g., sampleFor=10s)" << Qt::endl;
    *m_out << "  quit | exit                  Exit application" << Qt::endl;
    *m_out << Qt::endl;
    *m_out << "Display Commands:" << Qt::endl;
    *m_out << "  timebase_ms <ms>             Set timebase per division (e.g., timebase_ms 50)" << Qt::endl;
    *m_out << "  scale <units>                Set vertical scale (e.g., scale 1.0)" << Qt::endl;
    *m_out << Qt::endl;
    *m_out << "Generator Commands (for generator sources):" << Qt::endl;
    *m_out << "  freq <Hz>                    Set frequency (e.g., freq 440)" << Qt::endl;
    *m_out << "  amplitude <value>            Set amplitude (e.g., amplitude 1.0)" << Qt::endl;
    *m_out << "  offset <value>               Set DC offset (e.g., offset 0.0)" << Qt::endl;
    *m_out << "  waveform <sine|square|triangle>  Set waveform type" << Qt::endl;
    *m_out << Qt::endl;
    *m_out << "Acquisition Commands (for acquisition sources):" << Qt::endl;
    *m_out << "  noiseLevel <0.0-1.0>         Set noise level (e.g., noiseLevel 0.1)" << Qt::endl;
    *m_out << Qt::endl;
    *m_out << "Info Commands:" << Qt::endl;
    *m_out << "  status                       Show current scope status" << Qt::endl;
    *m_out << "  help | ?                     Show this help" << Qt::endl;
    *m_out << Qt::endl;
    m_out->flush();
    return true;
}

void scpTerminalController::writeResponse(const QString& message) {
    if (m_out) {
        *m_out << Qt::endl << message << Qt::endl;
        m_out->flush();
    }
}

double scpTerminalController::parseTimeValue(const QString& valueStr, QString& unit) {
    QString str = valueStr;
    unit = "ms";  // Default to ms
    
    if (str.endsWith("ms", Qt::CaseInsensitive)) {
        str.chop(2);
        unit = "ms";
    } else if (str.endsWith("s", Qt::CaseInsensitive)) {
        str.chop(1);
        unit = "s";
    }
    
    bool ok = false;
    double value = str.toDouble(&ok);
    return ok ? value : -1.0;
}

