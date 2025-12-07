#pragma once
#include "scpDataSource.h"
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <QVector>

/*
 * scpMessageWaveSource
 * Converts a text message -> ASCII values -> stepped waveform samples.
 *
 * Behavior:
 *  - Each character is converted to its ASCII code (0..127).
 *  - Each character is held for charDurationMs milliseconds (20 ms default).
 *  - The class produces float samples (range 0..127) which the scope will render.
 */

class scpMessageWaveSource : public scpDataSource {
public:
    scpMessageWaveSource(const std::string& message = "", int sampleRate = 1000, int charDurationMs = 20);
    ~scpMessageWaveSource() override;

    // scpDataSource interface
    bool start() override;
    void stop() override;
    bool isActive() const override;
    int sampleRate() const override;
    int copyRecentSamples(int count, QVector<float>& out) override;

    // API
    void setMessage(const std::string& message);
    void setCharDurationMs(int ms); // e.g., 20
    void setSampleRate(int sr);     // e.g., 1000 Hz

private:
    void generateSamplesForMessage(); // creates sample buffer
    void runLoop();

    // configuration
    std::string message_;
    int sampleRateHz_;
    int charDurationMs_;

    // generated samples (one full message)
    std::vector<float> messageSamples_;

    // runtime
    std::thread worker_;
    std::atomic<bool> running_;
    mutable std::mutex lock_;

    // ring buffer (recent samples) for copyRecentSamples
    std::vector<float> ringBuffer_;
    size_t ringHead_; // next write index
};
