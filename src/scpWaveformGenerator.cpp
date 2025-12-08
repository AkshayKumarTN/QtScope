#include "scpWaveformGenerator.h"
#include <QString>
#include <QRandomGenerator>
#include <cmath>
#include <algorithm>

float scpWaveformGenerator::generateSample(double phase, WaveformType type, double dutyCycle) {
    // Ensure phase is in [0.0, 1.0)
    phase = phase - floor(phase);
    
    switch (type) {
        case Sine:
            return generateSineSample(phase);
        case Square:
            return generateSquareSample(phase);
        case Triangle:
            return generateTriangleSample(phase);
        case Sawtooth:
            return generateSawtoothSample(phase);
        case Pulse:
            return generatePulseSample(phase, dutyCycle);
        case Noise:
            return generateNoiseSample();
        default:
            return generateSineSample(phase);
    }
}

QVector<float> scpWaveformGenerator::generateCycle(int samples, WaveformType type, double dutyCycle) {
    QVector<float> cycle;
    cycle.resize(samples);
    
    for (int i = 0; i < samples; ++i) {
        double phase = static_cast<double>(i) / samples;
        cycle[i] = generateSample(phase, type, dutyCycle);
    }
    
    return cycle;
}

double scpWaveformGenerator::generateSamples(float* output, int numSamples,
                                              double sampleRate, double frequency,
                                              float amplitude, float offset,
                                              WaveformType type, double dutyCycle,
                                              double startPhase) {
    if (!output || numSamples <= 0) return startPhase;
    
    double phaseIncrement = frequency / sampleRate;
    double phase = startPhase;
    
    for (int i = 0; i < numSamples; ++i) {
        float sample = generateSample(phase, type, dutyCycle);
        output[i] = sample * amplitude + offset;
        
        phase += phaseIncrement;
        // Wrap phase to [0.0, 1.0)
        if (phase >= 1.0) phase -= 1.0;
    }
    
    return phase;
}

float scpWaveformGenerator::addNoise(float sample, float noiseLevel, unsigned int seed) {
    static QRandomGenerator* rng = nullptr;
    if (!rng) {
        if (seed == 0) {
            rng = QRandomGenerator::global();
        } else {
            rng = new QRandomGenerator(seed);
        }
    }
    
    // Generate noise in range [-noiseLevel, +noiseLevel]
    float noise = (rng->generateDouble() * 2.0 - 1.0) * noiseLevel;
    return sample + noise;
}

scpWaveformGenerator::WaveformType scpWaveformGenerator::waveformTypeFromString(const QString& name) {
    QString lower = name.toLower().trimmed();
    if (lower == "sine") return Sine;
    if (lower == "square") return Square;
    if (lower == "triangle") return Triangle;
    if (lower == "sawtooth" || lower == "saw") return Sawtooth;
    if (lower == "pulse") return Pulse;
    if (lower == "noise") return Noise;
    return Sine;  // Default
}

QString scpWaveformGenerator::waveformTypeToString(WaveformType type) {
    switch (type) {
        case Sine: return "Sine";
        case Square: return "Square";
        case Triangle: return "Triangle";
        case Sawtooth: return "Sawtooth";
        case Pulse: return "Pulse";
        case Noise: return "Noise";
        default: return "Unknown";
    }
}

float scpWaveformGenerator::generateSineSample(double phase) {
    return std::sin(2.0 * M_PI * phase);
}

float scpWaveformGenerator::generateSquareSample(double phase) {
    return (phase < 0.5) ? 1.0f : -1.0f;
}

float scpWaveformGenerator::generateTriangleSample(double phase) {
    if (phase < 0.5) {
        return 4.0f * static_cast<float>(phase) - 1.0f;  // -1 to +1 over first half
    } else {
        return 3.0f - 4.0f * static_cast<float>(phase);  // +1 to -1 over second half
    }
}

float scpWaveformGenerator::generateSawtoothSample(double phase) {
    return 2.0f * static_cast<float>(phase) - 1.0f;  // -1 to +1 linearly
}

float scpWaveformGenerator::generatePulseSample(double phase, double dutyCycle) {
    return (phase < dutyCycle) ? 1.0f : -1.0f;
}

float scpWaveformGenerator::generateNoiseSample() {
    static QRandomGenerator* rng = QRandomGenerator::global();
    return (rng->generateDouble() * 2.0 - 1.0);  // Random in [-1, 1]
}

