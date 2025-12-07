#include "scpScopeView.h"
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>

scpScopeView::scpScopeView(QWidget* parent)
    : QWidget(parent) {
    setAutoFillBackground(true);
    m_timer.setInterval(16); // ~60 FPS
    connect(&m_timer, &QTimer::timeout, this, &scpScopeView::onRefresh);
    m_timer.start();
}

void scpScopeView::setSource(scpDataSource* src) {
    // Disconnect old source
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }
    
    m_source = src;
    
    // Connect to samplesReady signal for real-time updates
    if (m_source) {
        connect(m_source, &scpDataSource::samplesReady,
                this, &scpScopeView::onSamplesReady,
                Qt::QueuedConnection);
        m_useSignalBuffer = true;
    }
}

void scpScopeView::setTotalTimeWindowSec(double sec10Div) {
    m_timeWindowSec = sec10Div;
}

void scpScopeView::setVerticalScale(float unitsPerDiv) {
    m_unitsPerDiv = unitsPerDiv;
}

void scpScopeView::onRefresh() {
    update();
}

void scpScopeView::onSamplesReady(const float* data, int count) {
    if (!data || count <= 0) return;
    
    QMutexLocker lock(&m_bufferMutex);
    
    // Append new samples to signal buffer
    for (int i = 0; i < count; ++i) {
        m_signalBuffer.append(data[i]);
    }
    
    // Keep buffer size reasonable (last 50000 samples = ~50 seconds at 1kHz)
    const int maxBufferSize = 50000;
    while (m_signalBuffer.size() > maxBufferSize) {
        m_signalBuffer.removeFirst();
    }
}

void scpScopeView::paintEvent(QPaintEvent* e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.fillRect(rect(), palette().base());
    drawGrid(p);

    if (!m_source) {
        p.setPen(Qt::DashLine);
        p.drawText(rect().adjusted(10,10,-10,-10), Qt::AlignLeft | Qt::AlignTop, "No source configured");
        return;
    }

    if (!m_source->isActive()) {
        p.setPen(Qt::DashLine);
        p.drawText(rect().adjusted(10,10,-10,-10), Qt::AlignLeft | Qt::AlignTop, "Source stopped - click Start");
        return;
    }

    if (m_source->sampleRate() <= 0) {
        p.setPen(Qt::DashLine);
        p.drawText(rect().adjusted(10,10,-10,-10), Qt::AlignLeft | Qt::AlignTop, "Invalid sample rate");
        return;
    }

    const int sr = m_source->sampleRate();
    const int needed = std::max(100, static_cast<int>(std::ceil(sr * m_timeWindowSec)));
    QVector<float> samples;
    int got = 0;

    // Try signal-based buffer first (for message waveform and other signal sources)
    if (m_useSignalBuffer) {
        QMutexLocker lock(&m_bufferMutex);
        if (m_signalBuffer.size() >= needed) {
            // Copy the most recent 'needed' samples
            samples.resize(needed);
            int startIdx = m_signalBuffer.size() - needed;
            for (int i = 0; i < needed; ++i) {
                samples[i] = m_signalBuffer[startIdx + i];
            }
            got = needed;
        } else if (!m_signalBuffer.isEmpty()) {
            // Use whatever we have
            samples = m_signalBuffer;
            got = samples.size();
        }
    }

    // Fallback to polling method (for sources that don't emit signals)
    if (got == 0) {
        got = m_source->copyRecentSamples(needed, samples);
    }

    if (got > 0) {
        drawWave(p, samples);
    } else {
        p.setPen(Qt::DashLine);
        p.drawText(rect().adjusted(10,10,-10,-10), Qt::AlignLeft | Qt::AlignTop, 
                   "Waiting for data... (source active but no samples yet)");
    }
}

void scpScopeView::drawGrid(QPainter& p) {
    const QRect r = rect().adjusted(8, 8, -8, -8);
    const int divsX = 10;
    const int divsY = 8;

    // Outer frame
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(r);

    // Grid lines
    p.setPen(QPen(Qt::gray, 1, Qt::DotLine));
    for (int i = 1; i < divsX; ++i) {
        int x = r.left() + (i * r.width()) / divsX;
        p.drawLine(x, r.top(), x, r.bottom());
    }
    for (int j = 1; j < divsY; ++j) {
        int y = r.top() + (j * r.height()) / divsY;
        p.drawLine(r.left(), y, r.right(), y);
    }

    // Center line thicker
    p.setPen(QPen(Qt::black, 2));
    int yMid = r.center().y();
    p.drawLine(r.left(), yMid, r.right(), yMid);

    // Labels
    p.setPen(Qt::black);
    const QString tLabel = QString("Time/div: %1 ms").arg((m_timeWindowSec / 10.0) * 1000.0, 0, 'f', 2);
    const QString vLabel = QString("Units/div: %1").arg(m_unitsPerDiv, 0, 'f', 2);
    p.drawText(r.adjusted(4, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop, tLabel + "    " + vLabel);
}

void scpScopeView::drawWave(QPainter& p, const QVector<float>& samples) {
    const QRect r = rect().adjusted(8, 8, -8, -8);
    if (r.width() <= 1 || r.height() <= 1) return;

    const int N = samples.size();
    if (N == 0) return;

    const int W = r.width();
    
    // For message waveform (ASCII values 0-127), we need special handling
    // Check if values look like ASCII (mostly in 0-127 range)
    bool looksLikeASCII = true;
    int checkCount = std::min(100, N);
    for (int i = 0; i < checkCount; ++i) {
        if (samples[i] < 0 || samples[i] > 255) {
            looksLikeASCII = false;
            break;
        }
    }

    // Determine scaling approach
    float displayScale = 1.0f;
    float centerOffset = 0.0f;
    
    if (looksLikeASCII) {
        // ASCII mode: center around 64 (middle of ASCII range)
        // Scale so full range (0-127) fits nicely in display
        centerOffset = 64.0f;
        displayScale = m_unitsPerDiv / 16.0f; // 16 ASCII units per div
    } else {
        // Normal signal mode: use standard scaling
        displayScale = m_unitsPerDiv;
    }

    // Map samples -> pixels using min/max envelope per pixel to reduce aliasing
    const int step = std::max(1, N / W);

    QVector<QPointF> poly;
    poly.reserve(2 * (N / step));

    const float unitsPerDiv = m_unitsPerDiv;
    const float unitsPerScreen = unitsPerDiv * 8; // 8 vertical divisions
    
    for (int x = 0; x < W; ++x) {
        int start = x * step;
        int end = std::min(N, start + step);
        if (start >= end) break;
        
        float vmin =  1e9f;
        float vmax = -1e9f;
        
        for (int i = start; i < end; ++i) {
            // Apply centering and scaling
            float v = (samples[i] - centerOffset) / displayScale;
            
            // Clamp to reasonable range
            v = std::max(-20.0f, std::min(20.0f, v));
            
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
        
        // Convert to pixel Y (0 at top)
        auto toY = [&](float v) {
            float normalized = v / (unitsPerScreen / 2.0f); // -1..1 across half screen
            float y = r.center().y() - normalized * (r.height() / 2.0f);
            return y;
        };
        
        float yMin = toY(vmin);
        float yMax = toY(vmax);
        
        // Ensure min/max are ordered correctly for drawing
        if (yMin > yMax) std::swap(yMin, yMax);
        
        poly.push_back(QPointF(r.left() + x, yMin));
        poly.push_back(QPointF(r.left() + x, yMax));
    }

    // Draw waveform
    p.setPen(QPen(Qt::darkGreen, 1));
    for (int i = 0; i + 1 < poly.size(); i += 2) {
        p.drawLine(poly[i], poly[i+1]);
    }
    
    // Draw connection lines between min/max pairs for smoother appearance
    if (poly.size() >= 4) {
        p.setPen(QPen(Qt::darkGreen, 1));
        for (int i = 0; i + 3 < poly.size(); i += 2) {
            // Connect max of current pixel to min of next pixel
            p.drawLine(poly[i+1], poly[i+2]);
        }
    }
}