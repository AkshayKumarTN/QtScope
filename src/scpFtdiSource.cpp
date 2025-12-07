#include "scpFtdiSource.h"
#include <chrono>

scpFtdiSource::scpFtdiSource(const std::string& serial, size_t bufferSize)
    : handle_(nullptr), serial_(serial), bufferSize_(bufferSize),
      buffer_(bufferSize, 0.0f), running_(false),
      sampleRate_(1000), freq_(10.0), amp_(1.0f), phase_(0.0)
{}

scpFtdiSource::~scpFtdiSource() {
    stop();
    if (handle_) FT_Close(handle_);
}

bool scpFtdiSource::start() {
    if (running_) return false;

    FT_STATUS status = FT_OpenEx((void*)serial_.c_str(), FT_OPEN_BY_SERIAL_NUMBER, &handle_);
    if (status != FT_OK) {
        std::cerr << "Failed to open FTDI device: " << serial_ << "\n";
        handle_ = nullptr;
        return false;
    }

    FT_SetLatencyTimer(handle_, 2);
    FT_SetTimeouts(handle_, 5000, 5000);
    FT_SetBitMode(handle_, 0xFF, 0x40); // synchronous FIFO

    running_ = true;
    worker_ = std::thread(&scpFtdiSource::runLoop, this);
    return true;
}

void scpFtdiSource::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

bool scpFtdiSource::isActive() const { return running_; }

int scpFtdiSource::sampleRate() const { return sampleRate_; }

int scpFtdiSource::copyRecentSamples(int count, QVector<float>& out) {
    std::lock_guard<std::mutex> lock(bufMutex_);
    count = std::min(count, (int)buffer_.size());
    out.resize(count);
    for (int i = 0; i < count; ++i) out[i] = buffer_[i];
    return count;
}

void scpFtdiSource::sendText(const std::string& text) {
    if (!handle_) return;
    DWORD bytesWritten = 0;
    FT_Write(handle_, (void*)text.data(), (DWORD)text.size(), &bytesWritten);
}

void scpFtdiSource::setSignalFrequency(double hz) { freq_ = hz; }
void scpFtdiSource::setSignalAmplitude(float amp) { amp_ = amp; }

void scpFtdiSource::runLoop() {
    const double dt = 1.0 / sampleRate_;
    std::vector<uint8_t> rawBuffer(bufferSize_);

    while (running_) {
        {
            std::lock_guard<std::mutex> lock(bufMutex_);
            for (size_t i = 0; i < buffer_.size(); ++i) {
                buffer_[i] = amp_ * std::sin(phase_); // sine wave
                rawBuffer[i] = static_cast<uint8_t>((buffer_[i] + amp_) * 127.0f / amp_); // 0-255
                phase_ += 2.0 * M_PI * freq_ * dt;
                if (phase_ > 2.0 * M_PI) phase_ -= 2.0 * M_PI;
            }
        }

        // Write raw bytes to FTDI
        if (handle_) {
            DWORD bytesWritten = 0;
            FT_Write(handle_, rawBuffer.data(), (DWORD)rawBuffer.size(), &bytesWritten);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
