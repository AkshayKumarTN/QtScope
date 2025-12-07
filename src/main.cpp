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
#include "scpFtdiSource.h"
#include "scpMessageWaveSource.h"

static bool wantsTerminal(int argc, char* argv[]) {
    for (int i = 0; i < argc; ++i) {
        QString a = argv[i];
        if (a == "--view=terminal" || a == "--terminal" || a == "-T") return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    const bool terminal = wantsTerminal(argc, argv);

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
    QCommandLineOption sourceOpt(QStringList() << "s" << "source", "Source: audio | gen | ftdi | msg", "source", "audio");
    QCommandLineOption ftdiOpt(QStringList() << "ftdi", "FTDI serial number (for --source=ftdi)", "serial");
    QCommandLineOption startOpt(QStringList() << "S" << "start", "Start acquisition immediately");
    QCommandLineOption timebaseOpt(QStringList() << "t" << "timebase", "Timebase index (0..5)", "idx");
    QCommandLineOption scaleOpt(QStringList() << "V" << "scale", "Vertical scale index (0..4)", "idx");
    QCommandLineOption genFreqOpt(QStringList() << "f" << "gen-freq", "Generator frequency (Hz)", "hz");
    QCommandLineOption sizeOpt(QStringList() << "size", "Initial window size WxH (e.g. 1200x700)", "wxh");
    QCommandLineOption msgOpt(QStringList() << "msg", "Message text (for message source or display)", "text");

    parser.addOption(viewOpt);
    parser.addOption(sourceOpt);
    parser.addOption(ftdiOpt);
    parser.addOption(startOpt);
    parser.addOption(timebaseOpt);
    parser.addOption(scaleOpt);
    parser.addOption(genFreqOpt);
    parser.addOption(sizeOpt);
    parser.addOption(msgOpt);
    parser.process(app);

    const QString view = parser.value(viewOpt).toLower();
    const QString sourceStr = parser.value(sourceOpt).toLower();
    const QString msg = parser.value(msgOpt);

    // instantiate common sources
    scpAudioInputSource audio;
    scpSignalGeneratorSource gen;
    scpMessageWaveSource msgSource(msg.toStdString(), 1000, 20); // default 1000Hz sample, 20ms/char
    std::unique_ptr<scpFtdiSource> ftdi;

    scpDataSource* src = &audio; // default

    if (sourceStr == "gen") {
        src = &gen;
    } else if (sourceStr == "ftdi") {
        QString serial = parser.value(ftdiOpt);
        if (serial.isEmpty()) {
            qCritical() << "FTDI serial required with --source=ftdi";
            return 1;
        }
        ftdi = std::make_unique<scpFtdiSource>(serial.toStdString(), 256);
        src = ftdi.get();
    } else if (sourceStr == "msg") {
        // update message if provided
        if (!msg.isEmpty()) msgSource.setMessage(msg.toStdString());
        src = &msgSource;
    } else {
        src = &audio;
    }

    const bool doStart = parser.isSet(startOpt);
    if (doStart) src->start();

    // Terminal mode
    if (view == "terminal") {
        scpViewTerminal term;
        term.setSource(src);
        term.setTotalTimeWindowSec(0.5);
        term.setVerticalScale(1.0f);

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
            if (ok) gen.setFrequency(hz);
        }

        term.start();
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
