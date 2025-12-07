#include "scpMessageWaveSource.h"
#include <chrono>
#include <cmath>
#include <iostream>

scpMessageWaveSource::scpMessageWaveSource(const std::string& message, int sampleRate, int charDurationMs)
    : message_(message),
      sampleRateHz_(sampleRate),
      charDurationMs_(charDurationMs),
      running_(false),
      ringHead_(0)
{
    // allocate ring buffer to hold a few message repeats (e.g., 10 * message length)
    ringBuffer_.assign(std::max(1024, sampleRateHz_ * 2), 0.0f);
    generateSamplesForMessage();
}

scpMessageWaveSource::~scpMessageWaveSource() {
    stop();
}

void scpMessageWaveSource::setMessage(const std::string& message) {
    std::lock_guard<std::mutex> g(lock_);
    message_ = message;
    generateSamplesForMessage();
}

void scpMessageWaveSource::setCharDurationMs(int ms) {
    std::lock_guard<std::mutex> g(lock_);
    charDurationMs_ = ms;
    generateSamplesForMessage();
}

void scpMessageWaveSource::setSampleRate(int sr) {
    std::lock_guard<std::mutex> g(lock_);
    sampleRateHz_ = sr;
    // re-allocate ring buffer for reasonable size
    ringBuffer_.assign(std::max(1024, sampleRateHz_ * 2), 0.0f);
    generateSamplesForMessage();
}

bool scpMessageWaveSource::start() {
    if (running_) return false;
    running_ = true;
    worker_ = std::thread(&scpMessageWaveSource::runLoop, this);
    return true;
}

void scpMessageWaveSource::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

bool scpMessageWaveSource::isActive() const {
    return running_;
}

int scpMessageWaveSource::sampleRate() const {
    return sampleRateHz_;
}

int scpMessageWaveSource::copyRecentSamples(int count, QVector<float>& out) {
    std::lock_guard<std::mutex> g(lock_);
    if (count <= 0) { out.clear(); return 0; }
    const size_t bufSize = ringBuffer_.size();
    out.resize(count);
    // copy latest 'count' samples from ring (most recent at ringHead_-1)
    for (int i = 0; i < count; ++i) {
        // index of sample to copy (oldest -> newest)
        size_t idx = (ringHead_ + bufSize - count + i) % bufSize;
        out[i] = ringBuffer_[idx];
    }
    return count;
}

void scpMessageWaveSource::generateSamplesForMessage() {
    // Create messageSamples_ as ASCII-stepped waveform.
    messageSamples_.clear();
    if (message_.empty()) {
        // single zero sample
        messageSamples_.push_back(0.0f);
        return;
    }

    // number of samples per character:
    int samplesPerChar = std::max(1, (sampleRateHz_ * charDurationMs_) / 1000);

    // build messageSamples_
    for (char ch : message_) {
        unsigned char u = static_cast<unsigned char>(ch);
        float amplitude = static_cast<float>(u); // [0..255], but typical ASCII <128
        for (int s = 0; s < samplesPerChar; ++s) {
            messageSamples_.push_back(amplitude);
        }
    }

    // ensure ring buffer is at least as big as messageSamples_
    if (ringBuffer_.size() < messageSamples_.size() * 2) {
        ringBuffer_.assign(std::max<size_t>(messageSamples_.size() * 2, 1024), 0.0f);
        ringHead_ = 0;
    }
}

void scpMessageWaveSource::runLoop() {
    // push messageSamples_ repeatedly into ring buffer and call emitData on the new chunk
    const std::chrono::milliseconds sleepMs(10); // approximate loop sleep
    size_t msgLen = 0;
    {
        std::lock_guard<std::mutex> g(lock_);
        msgLen = messageSamples_.size();
    }
    if (msgLen == 0) {
        // nothing to do
        while (running_) std::this_thread::sleep_for(sleepMs);
        return;
    }

    size_t pos = 0;
    while (running_) {
        // copy a small chunk each iteration to the ring buffer and emit
        size_t chunk = std::min<size_t>(msgLen - pos, 128); // emit up to 128 samples at a time
        std::vector<float> outChunk;
        outChunk.reserve(chunk);

        {
            std::lock_guard<std::mutex> g(lock_);
            // ensure messageSamples_ current
            if (messageSamples_.empty()) {
                // regenerate
                generateSamplesForMessage();
            }
            // fill outChunk from messageSamples_
            for (size_t i = 0; i < chunk; ++i) {
                float samp = messageSamples_[(pos + i) % messageSamples_.size()];
                outChunk.push_back(samp);
                // write to ring buffer
                ringBuffer_[ringHead_] = samp;
                ringHead_ = (ringHead_ + 1) % ringBuffer_.size();
            }
        }

        // Emit data to scope (emitData expects float* and count)
        if (!outChunk.empty()) {
            // normalize or scale? We'll emit raw ASCII values; the scope's vertical scale can interpret units.
            // convert to float array pointer
            emit samplesReady(outChunk.data(), outChunk.size());
        }

        pos += chunk;
        if (pos >= msgLen) pos = 0; // loop message

        // sleep proportionally to number of samples emitted
        double seconds = static_cast<double>(chunk) / static_cast<double>(sampleRateHz_);
        auto sleep_time = std::chrono::duration<double>(seconds);
        // but to avoid too small sleeps, clamp to 5ms minimum
        if (sleep_time < std::chrono::milliseconds(5)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } else {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}
