#include "scpViewTerminal.h"
#include "scpDataSource.h"
#include "scpSignalGeneratorSource.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#ifdef Q_OS_UNIX
#include <QSocketNotifier>
#include <unistd.h>
#endif
#include <algorithm>
#include <cmath>

scpViewTerminal::scpViewTerminal(QObject* parent)
    : QObject(parent),
      m_out(stdout),
      m_in(stdin) {
    m_timer.setInterval(100); // 10 FPS
    QObject::connect(&m_timer, &QTimer::timeout, this, &scpViewTerminal::onTick);

#ifdef Q_OS_UNIX
    m_stdinNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(m_stdinNotifier, &QSocketNotifier::activated, this, &scpViewTerminal::onStdinActivity);
#endif

    m_useAnsi = !qEnvironmentVariableIsEmpty("TERM");
    printHelp();
}

void scpViewTerminal::setSource(scpDataSource* src) {
    m_source = src;
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
    if (!m_source || !m_source->isActive() || m_source->sampleRate() <= 0) {
        m_out << "[TerminalView] No active source. Type 'start' or 'help'." << Qt::endl;
        return;
    }
    const int sr = m_source->sampleRate();
    const int needed = std::max(100, (int)std::ceil(sr * m_timeWindowSec));
    QVector<float> s;
    int got = m_source->copyRecentSamples(needed, s);
    if (got > 0) printFrame(s);
}

void scpViewTerminal::printHelp() {
    m_out << "SimpleScope Terminal View (commands):" << Qt::endl;
    m_out << "  start | stop | quit" << Qt::endl;
    m_out << "  timebase_ms <per_div_ms>   (e.g., 50)" << Qt::endl;
    m_out << "  scale <units_per_div>      (e.g., 1.0)" << Qt::endl;
    m_out << "  freq <Hz>                  (for generator source)" << Qt::endl;
    m_out << Qt::endl;
}

void scpViewTerminal::printFrame(const QVector<float>& samples) {
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

    if (m_useAnsi) {
        m_out << "\x1b[2J\x1b[H"; // clear + home
    }
    m_out << "Time/div: " << QString::number((m_timeWindowSec/10.0) * 1000.0, 'f', 1)
          << " ms    Units/div: " << QString::number(m_unitsPerDiv, 'f', 2)
          << "    " << QDateTime::currentDateTime().toString("HH:mm:ss")
          << Qt::endl;

    for (int r=0; r<height; ++r) {
        m_out << QString::fromLatin1(grid.data() + r*width, width) << Qt::endl;
    }
    m_out.flush();
}

void scpViewTerminal::onStdinActivity() {
#ifdef Q_OS_UNIX
    QString line = m_in.readLine().trimmed();
    if (line.isEmpty()) return;
    const QString cmd = line.section(' ', 0, 0).toLower();
    const QString arg = line.section(' ', 1);

    if (cmd == "quit" || cmd == "exit") {
        QCoreApplication::quit();
        return;
    } else if (cmd == "start") {
        if (m_source && !m_source->isActive()) m_source->start();
    } else if (cmd == "stop") {
        if (m_source && m_source->isActive()) m_source->stop();
    } else if (cmd == "timebase_ms") {
        bool ok=false; double msdiv = arg.toDouble(&ok);
        if (ok && msdiv > 0.0) setTotalTimeWindowSec((msdiv/1000.0)*10.0);
    } else if (cmd == "scale") {
        bool ok=false; double u = arg.toDouble(&ok);
        if (ok && u > 0.0) setVerticalScale((float)u);
    } else if (cmd == "freq") {
        auto gen = dynamic_cast<class scpSignalGeneratorSource*>(m_source);
        if (gen) {
            bool ok=false; double hz = arg.toDouble(&ok);
            if (ok && hz>0) gen->setFrequency(hz);
        } else {
            m_out << "(freq) Only works when source is generator" << Qt::endl;
        }
    } else if (cmd == "help") {
        printHelp();
    } else {
        m_out << "Unknown command. Type 'help'." << Qt::endl;
    }
#endif
}
