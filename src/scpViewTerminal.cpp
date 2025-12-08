#include "scpViewTerminal.h"
#include "scpTerminalController.h"
#include "scpDataSource.h"
#include "scpSignalGeneratorSource.h"
#include "scpSimulatedGeneratorSource.h"
#include "scpSimulatedAcquisitionSource.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QThread>
#ifdef Q_OS_UNIX
#include <QSocketNotifier>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif
#include <algorithm>
#include <cmath>

scpViewTerminal::scpViewTerminal(QObject* parent)
    : QObject(parent),
      m_out(stdout),
      m_in(stdin) {
    m_timer.setInterval(200); // ~5 FPS - slower to allow typing without interference
    QObject::connect(&m_timer, &QTimer::timeout, this, &scpViewTerminal::onTick);

    // Initialize controller for command processing
    m_controller = new scpTerminalController(this);
    m_controller->setView(this);
    m_controller->setOutputStream(&m_out);
    
    // Connect controller signals
    connect(m_controller, &scpTerminalController::startRequested, this, &scpViewTerminal::onControllerStartRequested);
    connect(m_controller, &scpTerminalController::stopRequested, this, &scpViewTerminal::onControllerStopRequested);
    connect(m_controller, &scpTerminalController::quitRequested, this, &scpViewTerminal::onControllerQuitRequested);
    connect(m_controller, &scpTerminalController::viewUpdateNeeded, this, &scpViewTerminal::onControllerViewUpdateNeeded);
    
    // Get sampleFor timer from controller
    m_sampleForTimer = m_controller->sampleForTimer();

#ifdef Q_OS_UNIX
    // Use polling timer for better compatibility on macOS/Linux
    // QSocketNotifier may not work well when stdout is being updated frequently
    m_stdinPollTimer = new QTimer(this);
    connect(m_stdinPollTimer, &QTimer::timeout, this, &scpViewTerminal::onStdinActivity);
    m_stdinPollTimer->start(100);  // Check every 100ms
    
    // Also try QSocketNotifier as backup (but prefer polling)
    m_stdinNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(m_stdinNotifier, &QSocketNotifier::activated, this, &scpViewTerminal::onStdinActivity);
#elif defined(Q_OS_WIN)
    // On Windows, poll stdin periodically
    m_stdinPollTimer = new QTimer(this);
    connect(m_stdinPollTimer, &QTimer::timeout, this, &scpViewTerminal::onStdinActivity);
    m_stdinPollTimer->start(100);  // Check every 100ms
#endif

    m_useAnsi = !qEnvironmentVariableIsEmpty("TERM");
    printHelp();
}

scpViewTerminal::~scpViewTerminal() {
    // Qt parent-child relationship handles cleanup
}

void scpViewTerminal::setSource(scpDataSource* src) {
    m_source = src;
    // Update controller with the source
    if (m_controller) {
        m_controller->setSource(src);
    }
}

void scpViewTerminal::setAcquisitionSource(scpDataSource* src) {
    m_acquisitionSource = src;
    // Update controller
    if (m_controller) {
        m_controller->setAcquisitionSource(src);
    }
}

void scpViewTerminal::setGeneratorSource(scpDataSource* src) {
    m_generatorSource = src;
    // Update controller
    if (m_controller) {
        m_controller->setGeneratorSource(src);
    }
}

void scpViewTerminal::start() {
    m_timer.start();
    if (m_source && !m_source->isActive()) m_source->start();
}

void scpViewTerminal::stop() {
    m_timer.stop();
    if (m_source && m_source->isActive()) m_source->stop();
}

void scpViewTerminal::onTick() {
    // CRITICAL: Don't update display if user is typing - this prevents input being erased
    if (m_isTyping) {
        return;  // Exit immediately if typing detected
    }
    
    // Check for combined mode
    bool inCombinedMode = (m_acquisitionSource && m_acquisitionSource->isActive()) &&
                          (m_generatorSource && m_generatorSource->isActive());
    
    if (inCombinedMode) {
        // In combined mode, display acquisition source (or combine both)
        const int sr = m_acquisitionSource->sampleRate();
        const int needed = std::max(100, (int)std::ceil(sr * m_timeWindowSec));
        QVector<float> s;
        int got = m_acquisitionSource->copyRecentSamples(needed, s);
        if (got > 0) {
            printFrame(s);
        }
        return;
    }
    
    // Single source mode
    if (!m_source || !m_source->isActive() || m_source->sampleRate() <= 0) {
        // Don't spam the message, only show once
        static bool shown = false;
        if (!shown && !m_isTyping) {
            m_out << "[TerminalView] No active source. Type 'scope start' or 'help'." << Qt::endl;
            m_out.flush();
            shown = true;
        }
        return;
    }
    // Reset the flag when source becomes active
    static bool shown = false;
    shown = false;
    const int sr = m_source->sampleRate();
    // Calculate how many samples we need based on time window
    // m_timeWindowSec is total time for 10 divisions
    const int needed = std::max(100, (int)std::ceil(sr * m_timeWindowSec));
    QVector<float> s;
    int got = m_source->copyRecentSamples(needed, s);
    if (got > 0) {
        printFrame(s);
        // Debug info (optional - can be removed)
        // The waveform will visibly change - wider window shows more data points
    }
}

void scpViewTerminal::printHelp() {
    // Clear screen first if using ANSI
    if (m_useAnsi) {
        m_out << "\x1b[2J\x1b[H";  // Clear screen, move to top
    }
    m_out << "SimpleScope Terminal View (commands):" << Qt::endl;
    m_out << "  scope start | start            - Start acquisition" << Qt::endl;
    m_out << "  scope stop | stop              - Stop acquisition" << Qt::endl;
    m_out << "  scope sampleTime=<value>[ms|s] - Set time window (e.g., scope sampleTime=1ms)" << Qt::endl;
    m_out << "  scope sampleFor=<value>[s|ms]  - Sample for duration (e.g., scope sampleFor=10s)" << Qt::endl;
    m_out << "  status                         - Show current settings (sampleTime, frequency, etc.)" << Qt::endl;
    m_out << "  timebase_ms <per_div_ms>   (e.g., 50)" << Qt::endl;
    m_out << "  scale <units_per_div>      (e.g., 1.0)" << Qt::endl;
    m_out << "  freq <Hz>                  (for generator sources)" << Qt::endl;
    m_out << "  amplitude <value>          (for generator sources)" << Qt::endl;
    m_out << "  offset <value>             (for generator sources)" << Qt::endl;
    m_out << "  waveform <sine|square|triangle>  (for generator sources)" << Qt::endl;
    m_out << "  noiseLevel <0.0-1.0>       (for acquisition sources)" << Qt::endl;
    m_out << Qt::endl;
}

void scpViewTerminal::printFrame(const QVector<float>& samples) {
    // Don't update display if user is typing
    if (m_isTyping) {
        return;
    }
    
    const int width = 80;
    const int height = 20;
    const float unitsPerScreen = m_unitsPerDiv * 8.0f; // 8 divs vertically

    // Prepare a buffer of spaces
    std::vector<char> grid(width * height, ' ');

    auto toYrow = [&](float v) {
        float normalized = (v / (unitsPerScreen/2.0f)); // -1..1 across half-screen
        float y = (height/2.0f) - normalized * (height/2.0f);
        int row = std::clamp((int)std::round(y), 0, height-1);
        return row;
    };

    // Center line
    int mid = height/2;
    for (int x=0;x<width;++x) grid[mid*width + x] = '-';

    // Decimate to columns by min/max envelope
    const int N = samples.size();
    const int step = std::max(1, N / width);
    for (int x=0; x<width; ++x) {
        int start = x * step;
        int end = std::min(N, start + step);
        if (start >= end) break;
        float vmin =  1e9f, vmax = -1e9f;
        for (int i = start; i < end; ++i) {
            float v = samples[i] * (1.0f / m_unitsPerDiv);
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
        int r1 = toYrow(vmin);
        int r2 = toYrow(vmax);
        if (r1 > r2) std::swap(r1, r2);
        for (int r=r1; r<=r2; ++r) {
            grid[r*width + x] = '*';
        }
    }

    // CRITICAL: Never clear screen if user is typing
    if (m_isTyping) {
        return;  // Don't even try to update display
    }
    
    // Only clear/update if not typing
    if (m_useAnsi) {
        // Move cursor to top-left and clear entire screen first
        m_out << "\x1b[1;1H";  // Go to row 1, col 1
        m_out << "\x1b[2J";    // Clear entire screen
        m_out << "\x1b[1;1H";   // Back to top
    } else {
        m_out << Qt::endl << Qt::endl;  // Add some spacing if no ANSI
    }
    
    // Print header (keep it short to fit on one line - max 80 chars)
    double timePerDiv = (m_timeWindowSec / 10.0) * 1000.0;  // Convert to ms
    QString header = QString("Time/div: %1 ms    Units/div: %2    %3")
                     .arg(timePerDiv, 0, 'f', 1)
                     .arg(m_unitsPerDiv, 0, 'f', 2)
                     .arg(QDateTime::currentDateTime().toString("HH:mm:ss"));
    m_out << header << Qt::endl;

    for (int r=0; r<height; ++r) {
        m_out << QString::fromLatin1(grid.data() + r*width, width) << Qt::endl;
    }
    
    // Add separator line - but only if not typing (to avoid overwriting commands)
    if (!m_isTyping) {
        m_out << "────────────────────────────────────────────────────────────────────────" << Qt::endl;
    }
    m_out.flush();
}

void scpViewTerminal::onStdinActivity() {
    QString line;
#ifdef Q_OS_UNIX
    // Check if data is available using select (non-blocking)
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;  // 50ms timeout - allow some time for full line input
    
    // Check for input with very short timeout
    int result = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
    if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        // CRITICAL: Stop display IMMEDIATELY when input detected
        m_isTyping = true;
        m_timer.stop();  // Stop ALL display updates
        m_out.flush();   // Make sure all output is flushed
        
        // Don't move cursor - just ensure we're ready for input
        // The prompt will appear when user types
        
        // Wait a bit to collect full line (especially if user is typing fast)
        QThread::msleep(10);
        
        // Check again if more data is coming
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // Wait up to 100ms for more input
        int result2 = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
        
        // Data is available, try to read
        char buffer[512];
        ssize_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            line = QString::fromLocal8Bit(buffer).trimmed();
            // Remove any trailing newlines
            line.remove('\r').remove('\n');
            if (line.isEmpty()) {
                // Resume display if empty line
                m_isTyping = false;
                if (m_source && m_source->isActive()) {
                    m_timer.start();
                }
                return;  // Empty line, ignore
            }
        } else {
            m_isTyping = false;
            if (m_source && m_source->isActive()) {
                m_timer.start();
            }
            return;
        }
    } else {
        return;  // No data available
    }
#elif defined(Q_OS_WIN)
    // Check if input is available on Windows
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD dwEvents;
    if (!PeekConsoleInput(hStdin, nullptr, 0, &dwEvents) || dwEvents == 0) {
        return;
    }
    // Read input
    char buffer[256];
    DWORD dwRead;
    if (ReadConsoleA(hStdin, buffer, sizeof(buffer)-1, &dwRead, nullptr)) {
        buffer[dwRead] = '\0';
        line = QString::fromLocal8Bit(buffer).trimmed();
        // Remove newline if present
        line.remove('\r').remove('\n');
    } else {
        return;
    }
#endif

    if (line.isEmpty()) {
        m_isTyping = false;
        // Resume display
        if (m_source && m_source->isActive()) {
            m_timer.start();
        }
        return;
    }
    
    // Process command through controller (MVC separation)
    if (m_controller) {
        m_controller->processCommand(line);
    }
    
    // Resume display after command processing
    m_isTyping = false;
}

void scpViewTerminal::onControllerStartRequested() {
    // Start the display timer when controller requests start
    // Check for combined mode or single source
    bool shouldStart = false;
    if (m_acquisitionSource && m_acquisitionSource->isActive() &&
        m_generatorSource && m_generatorSource->isActive()) {
        // Combined mode - start display
        shouldStart = true;
    } else if (m_source && m_source->isActive()) {
        // Single source mode
        shouldStart = true;
    }
    
    if (shouldStart && !m_timer.isActive()) {
        m_timer.start();
    }
}

void scpViewTerminal::onControllerStopRequested() {
    // Stop the display timer when controller requests stop
    m_timer.stop();
}

void scpViewTerminal::onControllerQuitRequested() {
    QCoreApplication::quit();
}

void scpViewTerminal::onControllerViewUpdateNeeded() {
    // View update requested by controller (e.g., after timebase/scale change)
    // Display will update on next onTick() automatically
}
