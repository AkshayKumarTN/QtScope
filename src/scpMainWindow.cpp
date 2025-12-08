#include "scpMainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QLabel>

static const struct { const char* label; double sec; } kTimebases[] = {
    {"5 ms/div", 0.005}, {"10 ms/div", 0.010}, {"20 ms/div", 0.020},
    {"50 ms/div", 0.050}, {"100 ms/div", 0.100}, {"200 ms/div", 0.200}
};

static const struct { const char* label; float units; } kScales[] = {
    {"0.2 units/div", 0.2f}, {"0.5 units/div", 0.5f}, {"1.0 units/div", 1.0f},
    {"2.0 units/div", 2.0f}, {"5.0 units/div", 5.0f}
};

scpMainWindow::scpMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
}

scpMainWindow::~scpMainWindow() {
    if (m_audio) m_audio->stop();
    if (m_gen) m_gen->stop();
    if (m_simGen) m_simGen->stop();
    if (m_simAcq) m_simAcq->stop();
    if (m_msgSource) m_msgSource->stop();
}

void scpMainWindow::buildUi() {
    m_view = new scpScopeView(this);

    // Controls - First Row
    m_controls = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(m_controls);
    mainLayout->setContentsMargins(8,8,8,8);
    mainLayout->setSpacing(8);

    // First row: Source selection and main controls
    auto* firstRow = new QWidget(m_controls);
    auto* hl1 = new QHBoxLayout(firstRow);
    hl1->setContentsMargins(0,0,0,0);
    hl1->setSpacing(8);

    m_sourceCombo = new QComboBox(firstRow);
    m_sourceCombo->addItem("Audio In (mic/line)");
    m_sourceCombo->addItem("Signal Generator (sine)");
    m_sourceCombo->addItem("Simulated Generator");
    m_sourceCombo->addItem("Simulated Acquisition");
    m_sourceCombo->addItem("Message Waveform");
    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &scpMainWindow::onSourceChanged);

    m_startStop = new QPushButton("Start", firstRow);
    connect(m_startStop, &QPushButton::clicked, this, &scpMainWindow::onStartStop);

    m_timebaseCombo = new QComboBox(firstRow);
    for (auto t : kTimebases) m_timebaseCombo->addItem(t.label);
    m_timebaseCombo->setCurrentIndex(3); // default 50 ms/div
    connect(m_timebaseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &scpMainWindow::onTimebaseChanged);

    m_scaleCombo = new QComboBox(firstRow);
    for (auto s : kScales) m_scaleCombo->addItem(s.label);
    m_scaleCombo->setCurrentIndex(2);
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &scpMainWindow::onScaleChanged);

    m_genFreq = new QDoubleSpinBox(firstRow);
    m_genFreq->setRange(1.0, 20000.0);
    m_genFreq->setDecimals(1);
    m_genFreq->setSuffix(" Hz");
    m_genFreq->setValue(440.0);
    connect(m_genFreq, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &scpMainWindow::onGenFreqChanged);

    m_waveformCombo = new QComboBox(firstRow);
    m_waveformCombo->addItem("Sine");
    m_waveformCombo->addItem("Square");
    m_waveformCombo->addItem("Triangle");
    connect(m_waveformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &scpMainWindow::onWaveformTypeChanged);

    m_genAmplitude = new QDoubleSpinBox(firstRow);
    m_genAmplitude->setRange(0.1, 10.0);
    m_genAmplitude->setDecimals(2);
    m_genAmplitude->setValue(1.0);
    connect(m_genAmplitude, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &scpMainWindow::onGenAmplitudeChanged);

    m_genOffset = new QDoubleSpinBox(firstRow);
    m_genOffset->setRange(-5.0, 5.0);
    m_genOffset->setDecimals(2);
    m_genOffset->setValue(0.0);
    m_genOffset->setSuffix(" V");
    connect(m_genOffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &scpMainWindow::onGenOffsetChanged);

    hl1->addWidget(new QLabel("Source:", firstRow));
    hl1->addWidget(m_sourceCombo);
    hl1->addWidget(m_startStop);
    hl1->addSpacing(12);
    hl1->addWidget(new QLabel("Timebase:", firstRow));
    hl1->addWidget(m_timebaseCombo);
    hl1->addSpacing(12);
    hl1->addWidget(new QLabel("Vertical:", firstRow));
    hl1->addWidget(m_scaleCombo);
    hl1->addSpacing(12);
    hl1->addWidget(new QLabel("Freq:", firstRow));
    hl1->addWidget(m_genFreq);
    hl1->addSpacing(12);
    hl1->addWidget(new QLabel("Waveform:", firstRow));
    hl1->addWidget(m_waveformCombo);
    hl1->addSpacing(12);
    hl1->addWidget(new QLabel("Amp:", firstRow));
    hl1->addWidget(m_genAmplitude);
    hl1->addSpacing(12);
    hl1->addWidget(new QLabel("Offset:", firstRow));
    hl1->addWidget(m_genOffset);
    hl1->addStretch(1);

    // Second row: Message input (only visible for Message Waveform source)
    auto* secondRow = new QWidget(m_controls);
    auto* hl2 = new QHBoxLayout(secondRow);
    hl2->setContentsMargins(0,0,0,0);
    hl2->setSpacing(8);

    m_msgInput = new QLineEdit(secondRow);
    m_msgInput->setPlaceholderText("Enter message to transmit as waveform...");
    m_msgInput->setText("HELLO WORLD");
    
    m_sendBtn = new QPushButton("Send Message", secondRow);
    m_sendBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 6px 12px; }");
    connect(m_sendBtn, &QPushButton::clicked, this, &scpMainWindow::onSendMessage);
    
    // Enable return key to send
    connect(m_msgInput, &QLineEdit::returnPressed, this, &scpMainWindow::onSendMessage);

    hl2->addWidget(new QLabel("Message:", secondRow));
    hl2->addWidget(m_msgInput, 1);  // stretch factor 1 to take available space
    hl2->addWidget(m_sendBtn);

    // Initially hide message row (show only when Message Waveform is selected)
    secondRow->setVisible(false);

    // Add both rows to main layout
    mainLayout->addWidget(firstRow);
    mainLayout->addWidget(secondRow);

    // Store secondRow as a member for show/hide (using dynamic property)
    m_controls->setProperty("messageRow", QVariant::fromValue(secondRow));

    auto* central = new QWidget(this);
    auto* vl = new QVBoxLayout(central);
    vl->setContentsMargins(0,0,0,0);
    vl->addWidget(m_view, /*stretch*/1);
    vl->addWidget(m_controls, /*stretch*/0);
    setCentralWidget(central);

    // Data sources
    m_audio = new scpAudioInputSource(this);
    m_gen = new scpSignalGeneratorSource(this);
    m_simGen = new scpSimulatedGeneratorSource(this);
    m_simAcq = new scpSimulatedAcquisitionSource(this);
    m_msgSource = new scpMessageWaveSource("HELLO WORLD", 1000, 20); // 20 ms per char default
    m_view->setSource(m_simAcq);  // Default to simulated acquisition for standalone
    m_current = m_simAcq;

    onTimebaseChanged(m_timebaseCombo->currentIndex());
    onScaleChanged(m_scaleCombo->currentIndex());

    // Status bar
    m_status = new QLabel(this);
    statusBar()->addPermanentWidget(m_status);
    m_status->setText("Ready");

    // Message label for GUI messages (left of status area)
    m_msgLabel = new QLabel(this);
    m_msgLabel->setStyleSheet("color: blue; font-weight: bold;");
    statusBar()->addWidget(m_msgLabel);
}

void scpMainWindow::onSourceChanged(int idx) {
    if (m_running) onStartStop(); // stop current
    
    // Show/hide message input based on source
    QWidget* messageRow = m_controls->property("messageRow").value<QWidget*>();
    if (messageRow) {
        messageRow->setVisible(idx == 4); // Show only for Message Waveform (index 4)
    }

    // Show/hide generator controls based on source
    bool isGenerator = (idx == 1 || idx == 2);  // Signal Generator or Simulated Generator
    m_waveformCombo->setVisible(isGenerator);
    m_genAmplitude->setVisible(isGenerator);
    m_genOffset->setVisible(isGenerator);
    if (idx == 1) {
        // Original generator doesn't support waveform type, amplitude, offset
        m_waveformCombo->setVisible(false);
        m_genAmplitude->setVisible(false);
        m_genOffset->setVisible(false);
    }
    
    if (idx == 0) {
        m_view->setSource(m_audio);
        m_current = m_audio;
        m_status->setText("Source: Audio In");
    } else if (idx == 1) {
        m_view->setSource(m_gen);
        m_current = m_gen;
        m_status->setText("Source: Signal Generator");
    } else if (idx == 2) {
        m_view->setSource(m_simGen);
        m_current = m_simGen;
        m_status->setText("Source: Simulated Generator");
        // Apply current settings
        onWaveformTypeChanged(m_waveformCombo->currentIndex());
        onGenAmplitudeChanged(m_genAmplitude->value());
        onGenOffsetChanged(m_genOffset->value());
    } else if (idx == 3) {
        m_view->setSource(m_simAcq);
        m_current = m_simAcq;
        m_status->setText("Source: Simulated Acquisition");
    } else if (idx == 4) {
        m_view->setSource(m_msgSource);
        m_current = m_msgSource;
        m_status->setText("Source: Message Waveform");
    }
}

void scpMainWindow::onStartStop() {
    if (!m_running) {
        if (m_current && m_current->start()) {
            m_running = true;
            m_startStop->setText("Stop");
            m_status->setText("Running");
        } else {
            m_status->setText("Failed to start source");
        }
    } else {
        if (m_current) m_current->stop();
        m_running = false;
        m_startStop->setText("Start");
        m_status->setText("Stopped");
    }
}

void scpMainWindow::onTimebaseChanged(int idx) {
    if (idx < 0) return;
    // total window seconds = sec/per_div * 10 divisions
    double totalSec = kTimebases[idx].sec * 10.0;
    m_view->setTotalTimeWindowSec(totalSec);
}

void scpMainWindow::onScaleChanged(int idx) {
    if (idx < 0) return;
    m_view->setVerticalScale(kScales[idx].units);
}

void scpMainWindow::onGenFreqChanged(double f) {
    if (m_gen) m_gen->setFrequency(f);
    if (m_simGen) m_simGen->setFrequency(f);
}

void scpMainWindow::onWaveformTypeChanged(int idx) {
    if (!m_simGen) return;
    switch (idx) {
        case 0:
            m_simGen->setWaveformType(scpSimulatedGeneratorSource::Sine);
            break;
        case 1:
            m_simGen->setWaveformType(scpSimulatedGeneratorSource::Square);
            break;
        case 2:
            m_simGen->setWaveformType(scpSimulatedGeneratorSource::Triangle);
            break;
    }
}

void scpMainWindow::onGenAmplitudeChanged(double amp) {
    if (m_simGen) m_simGen->setAmplitude(static_cast<float>(amp));
}

void scpMainWindow::onGenOffsetChanged(double offset) {
    if (m_simGen) m_simGen->setOffset(static_cast<float>(offset));
}

void scpMainWindow::onSendMessage() {
    QString msg = m_msgInput->text();
    if (msg.isEmpty()) {
        m_status->setText("Message is empty!");
        return;
    }
    
    if (m_msgSource) {
        m_msgSource->setMessage(msg.toStdString());
        m_msgLabel->setText("Message: " + msg);
        m_status->setText("Message updated: " + msg);
        
        // If not already running, start automatically
        if (!m_running && m_current == m_msgSource) {
            onStartStop();
        }
    }
}

void scpMainWindow::setSource(scpDataSource* source) {
    if (!source) return;
    m_current = source;
    m_view->setSource(source);
}

void scpMainWindow::showMessage(const QString& message) {
    if (m_msgLabel) {
        m_msgLabel->setText("Message: " + message);
    }
    if (m_msgInput) {
        m_msgInput->setText(message);
    }
    if (m_msgSource) {
        m_msgSource->setMessage(message.toStdString());
    }
}