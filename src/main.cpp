#include <memory>
#include <QCoreApplication>
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QString>
#include <QDebug>

#include "scpMainWindow.h"
#include "scpViewTerminal.h"
#include "scpAudioInputSource.h"
#include "scpSignalGeneratorSource.h"
#ifdef Q_OS_WIN
#include "scpFtdiSource.h"
#endif
#include "scpMessageWaveSource.h"
#include "scpSimulatedAcquisitionSource.h"
#include "scpSimulatedGeneratorSource.h"

static bool wantsTerminal(int argc, char* argv[]) {
    for (int i = 0; i < argc; ++i) {
        QString a = argv[i];
        if (a == "--view=terminal" || a == "--terminal" || a == "-T" || 
            a == "--cli" || a == "-c") return true;
        if (a.startsWith("--view=")) {
            QString view = a.mid(7).toLower();
            if (view == "terminal" || view == "cli") return true;
        }
    }
    return false;
}

int main(int argc, char *argv[]) {
    bool terminal = wantsTerminal(argc, argv);

    std::unique_ptr<QCoreApplication> coreApp;
    std::unique_ptr<QApplication> guiApp;
    if (terminal) {
        coreApp = std::make_unique<QCoreApplication>(argc, argv);
    } else {
        guiApp = std::make_unique<QApplication>(argc, argv);
    }
    QCoreApplication& app = terminal ? *coreApp : static_cast<QCoreApplication&>(*guiApp);

    QCoreApplication::setApplicationName("SimpleScope");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Simple Qt oscilloscope (GUI or terminal)");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption viewOpt(QStringList() << "view", "View: gui | terminal", "view", terminal ? "terminal" : "gui");
    QCommandLineOption cliOpt(QStringList() << "cli" << "c", "Use CLI/terminal view (alias for --view=terminal)");
    QCommandLineOption uiOpt(QStringList() << "ui" << "u", "Use GUI view (alias for --view=gui)");
    QString sourceHelp = "Source: audio | gen | msg | simacq | simgen";
#ifdef Q_OS_WIN
    sourceHelp += " | ftdi";
#endif
    QCommandLineOption sourceOpt(QStringList() << "s" << "source", 
                                 sourceHelp, 
                                 "source", "simacq");
#ifdef Q_OS_WIN
    QCommandLineOption ftdiOpt(QStringList() << "ftdi", "FTDI serial number (for --source=ftdi)", "serial");
#endif
    QCommandLineOption startOpt(QStringList() << "S" << "start", "Start acquisition immediately");
    QCommandLineOption timebaseOpt(QStringList() << "t" << "timebase", "Timebase index (0..5)", "idx");
    QCommandLineOption scaleOpt(QStringList() << "V" << "scale", "Vertical scale index (0..4)", "idx");
    QCommandLineOption genFreqOpt(QStringList() << "f" << "gen-freq", "Generator frequency (Hz)", "hz");
    QCommandLineOption sizeOpt(QStringList() << "size", "Initial window size WxH (e.g. 1200x700)", "wxh");
    QCommandLineOption msgOpt(QStringList() << "msg", "Message text (for message source or display)", "text");

    parser.addOption(viewOpt);
    parser.addOption(cliOpt);
    parser.addOption(uiOpt);
    parser.addOption(sourceOpt);
#ifdef Q_OS_WIN
    parser.addOption(ftdiOpt);
#endif
    parser.addOption(startOpt);
    parser.addOption(timebaseOpt);
    parser.addOption(scaleOpt);
    parser.addOption(genFreqOpt);
    parser.addOption(sizeOpt);
    parser.addOption(msgOpt);
    parser.process(app);

    // Determine final view mode
    if (parser.isSet(cliOpt)) {
        terminal = true;
    } else if (parser.isSet(uiOpt)) {
        terminal = false;
    } else {
        // Check --view option
        QString viewValue = parser.value(viewOpt).toLower();
        if (viewValue == "terminal" || viewValue == "cli") {
            terminal = true;
        } else if (viewValue == "gui" || viewValue == "ui") {
            terminal = false;
        }
    }

    const QString view = terminal ? "terminal" : "gui";
    const QString sourceStr = parser.value(sourceOpt).toLower();
    const QString msg = parser.value(msgOpt);

    // instantiate common sources
    scpAudioInputSource audio;
    scpSignalGeneratorSource gen;
    scpMessageWaveSource msgSource(msg.toStdString(), 1000, 20); // default 1000Hz sample, 20ms/char
#ifdef Q_OS_WIN
    std::unique_ptr<scpFtdiSource> ftdi;
#endif
    std::unique_ptr<scpSimulatedAcquisitionSource> simAcq;
    std::unique_ptr<scpSimulatedGeneratorSource> simGen;

    scpDataSource* src = nullptr; // default

    if (sourceStr == "gen") {
        src = &gen;
    } else if (sourceStr == "simgen") {
        simGen = std::make_unique<scpSimulatedGeneratorSource>();
        src = simGen.get();
    } else if (sourceStr == "simacq") {
        simAcq = std::make_unique<scpSimulatedAcquisitionSource>();
        src = simAcq.get();
#ifdef Q_OS_WIN
    } else if (sourceStr == "ftdi") {
        QString serial = parser.value(ftdiOpt);
        if (serial.isEmpty()) {
            qCritical() << "FTDI serial required with --source=ftdi";
            return 1;
        }
        ftdi = std::make_unique<scpFtdiSource>(serial.toStdString(), 256);
        src = ftdi.get();
#endif
    } else if (sourceStr == "msg") {
        // update message if provided
        if (!msg.isEmpty()) msgSource.setMessage(msg.toStdString());
        src = &msgSource;
    } else if (sourceStr == "audio") {
        src = &audio;
    } else {
        // Default: use simulated acquisition for standalone operation
        simAcq = std::make_unique<scpSimulatedAcquisitionSource>();
        src = simAcq.get();
    }

    const bool doStart = parser.isSet(startOpt);
    if (doStart) src->start();

    // Terminal mode
    if (view == "terminal") {
        scpViewTerminal term;
        term.setSource(src);
        term.setTotalTimeWindowSec(0.5);
        term.setVerticalScale(1.0f);
        
        // Set up sources for combined mode
        // Create both sources if not already created
        if (!simAcq) {
            simAcq = std::make_unique<scpSimulatedAcquisitionSource>();
        }
        if (!simGen) {
            simGen = std::make_unique<scpSimulatedGeneratorSource>();
        }
        
        // Set up combined mode sources
        term.setAcquisitionSource(simAcq.get());
        term.setGeneratorSource(simGen.get());

        if (!msg.isEmpty()) qInfo() << "Message:" << msg;

        if (parser.isSet(timebaseOpt)) {
            bool ok=false; int idx = parser.value(timebaseOpt).toInt(&ok);
            const double tb[] = {0.050*10,0.100*10,0.200*10,0.500*10,1.000*10,2.000*10};
            if (ok && idx>=0 && idx<6) term.setTotalTimeWindowSec(tb[idx]);
        }
        if (parser.isSet(scaleOpt)) {
            bool ok=false; int idx = parser.value(scaleOpt).toInt(&ok);
            const float sc[] = {0.2f,0.5f,1.0f,2.0f,5.0f};
            if (ok && idx>=0 && idx<5) term.setVerticalScale(sc[idx]);
        }
        if (parser.isSet(genFreqOpt)) {
            bool ok=false; double hz = parser.value(genFreqOpt).toDouble(&ok);
            if (ok) {
                if (gen.isActive() || sourceStr == "gen") {
                    gen.setFrequency(hz);
                }
                if (simGen && (simGen->isActive() || sourceStr == "simgen")) {
                    simGen->setFrequency(hz);
                }
            }
        }

        // Only auto-start if --start flag is explicitly set
        if (doStart) {
            term.start();
        }
        // Otherwise, wait for user to type "scope start" command
        return app.exec();
    }

    // GUI mode
    scpMainWindow win;
    win.show();

    // give GUI the selected source
    win.setSource(src);

    // show message on GUI label if provided
    if (!msg.isEmpty()) win.showMessage(msg);

    // apply options after window is shown
    QTimer::singleShot(0, [&](){
        if (doStart) QMetaObject::invokeMethod(&win, "onStartStop", Qt::DirectConnection);
        if (parser.isSet(timebaseOpt)) {
            bool ok=false; int idx = parser.value(timebaseOpt).toInt(&ok);
            if (ok) QMetaObject::invokeMethod(&win, "onTimebaseChanged", Qt::DirectConnection, Q_ARG(int, idx));
        }
        if (parser.isSet(scaleOpt)) {
            bool ok=false; int idx = parser.value(scaleOpt).toInt(&ok);
            if (ok) QMetaObject::invokeMethod(&win, "onScaleChanged", Qt::DirectConnection, Q_ARG(int, idx));
        }
        if (parser.isSet(genFreqOpt)) {
            bool ok=false; double hz = parser.value(genFreqOpt).toDouble(&ok);
            if (ok) QMetaObject::invokeMethod(&win, "onGenFreqChanged", Qt::DirectConnection, Q_ARG(double, hz));
        }
    });

    return app.exec();
}
