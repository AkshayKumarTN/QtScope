#pragma once
#include "scpDataSource.h"
#include <ftd2xx.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <mutex>
#include <cmath>

class scpFtdiSource : public scpDataSource {
public:
    scpFtdiSource(const std::string& serial, size_t bufferSize = 256);
    ~scpFtdiSource() override;

    // Start/stop acquisition
    bool start() override;
    void stop() override;

    // Send text as raw bytes
    void sendText(const std::string& text);

    // Implement pure virtuals
    bool isActive() const override;
    int sampleRate() const override; // Return sample rate
    int copyRecentSamples(int count, QVector<float>& out) override;

    // Set test signal parameters
    void setSignalFrequency(double hz);  // sine wave frequency
    void setSignalAmplitude(float amp);  // sine wave amplitude

private:
    void runLoop();

    FT_HANDLE handle_;
    std::string serial_;
    size_t bufferSize_;
    std::vector<float> buffer_;
    std::thread worker_;
    std::atomic<bool> running_;
    int sampleRate_; // store user-defined sample rate
    std::mutex bufMutex_;

    // Test signal generation
    double freq_;    // Hz
    float amp_;      // amplitude
    double phase_;   // internal phase for sine wave
};
