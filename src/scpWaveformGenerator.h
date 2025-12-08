#pragma once
#include <QVector>
#include <QString>
#include <cmath>
#include <algorithm>

/**
 * @brief Utility class for generating various waveform types
 * 
 * Provides static methods for generating waveform samples.
 * Can be used by any source that needs waveform generation.
 */
class scpWaveformGenerator {
public:
    enum WaveformType {
        Sine,
        Square,
        Triangle,
        Sawtooth,
        Pulse,      // Pulse wave with configurable duty cycle
        Noise       // Random noise
    };

    /**
     * @brief Generate a single sample of a waveform
     * @param phase Phase in range [0.0, 1.0) representing position in one cycle
     * @param type Waveform type
     * @param dutyCycle Duty cycle for pulse wave (0.0 to 1.0), default 0.5
     * @return Sample value in range typically [-1.0, 1.0]
     */
    static float generateSample(double phase, WaveformType type, double dutyCycle = 0.5);

    /**
     * @brief Generate a complete cycle of waveform samples
     * @param samples Number of samples per cycle
     * @param type Waveform type
     * @param dutyCycle Duty cycle for pulse wave (0.0 to 1.0)
     * @return Vector of samples for one complete cycle
     */
    static QVector<float> generateCycle(int samples, WaveformType type, double dutyCycle = 0.5);

    /**
     * @brief Generate waveform samples with frequency, amplitude, and offset
     * @param output Buffer to fill with samples
     * @param numSamples Number of samples to generate
     * @param sampleRate Sample rate in Hz
     * @param frequency Waveform frequency in Hz
     * @param amplitude Peak amplitude
     * @param offset DC offset
     * @param type Waveform type
     * @param dutyCycle Duty cycle for pulse wave
     * @param startPhase Starting phase [0.0, 1.0)
     * @return Current phase after generation (useful for continuous generation)
     */
    static double generateSamples(float* output, int numSamples,
                                  double sampleRate, double frequency,
                                  float amplitude, float offset,
                                  WaveformType type, double dutyCycle = 0.5,
                                  double startPhase = 0.0);

    /**
     * @brief Add noise to a sample
     * @param sample Original sample
     * @param noiseLevel Noise level (0.0 = no noise, 1.0 = full noise)
     * @param seed Random seed (0 = use time-based seed)
     * @return Sample with noise added
     */
    static float addNoise(float sample, float noiseLevel, unsigned int seed = 0);

    /**
     * @brief Convert waveform type name to enum
     * @param name Waveform name (case-insensitive): "sine", "square", "triangle", etc.
     * @return WaveformType enum, or Sine if name not recognized
     */
    static WaveformType waveformTypeFromString(const QString& name);

    /**
     * @brief Convert waveform type enum to string
     * @param type WaveformType enum
     * @return String name of waveform type
     */
    static QString waveformTypeToString(WaveformType type);

private:
    static float generateSineSample(double phase);
    static float generateSquareSample(double phase);
    static float generateTriangleSample(double phase);
    static float generateSawtoothSample(double phase);
    static float generatePulseSample(double phase, double dutyCycle);
    static float generateNoiseSample();
};

