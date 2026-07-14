#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

// Small shared helpers used across the Tests target. Kept dependency-free
// (just juce_audio_basics) so it can be included from any test file.
namespace TestHelpers
{
    // Fills every channel of the buffer with a sine wave of the given
    // frequency. `startSampleIndex` offsets the phase calculation, so
    // calling this for consecutive blocks with startSampleIndex incremented
    // by each block's length produces a phase-continuous sine across block
    // boundaries. Defaults to 0.
    inline void fillWithSine (juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              double frequencyHz,
                              float amplitude = 0.5f,
                              juce::int64 startSampleIndex = 0)
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto phase = juce::MathConstants<double>::twoPi * frequencyHz
                                    * static_cast<double> (startSampleIndex + sample) / sampleRate;
                data[sample] = amplitude * static_cast<float> (std::sin (phase));
            }
        }
    }

    // Fills a genuinely stereo (non-mono, L != R) test signal: left channel
    // gets `leftFrequencyHz`, right channel gets `rightFrequencyHz`. Used by
    // M/S round-trip and width tests, which need real inter-channel
    // difference content (a mono source would trivially have Side == 0 and
    // wouldn't exercise the width/bass-mono stages at all).
    inline void fillStereoWithDistinctSines (juce::AudioBuffer<float>& buffer,
                                              double sampleRate,
                                              double leftFrequencyHz,
                                              double rightFrequencyHz,
                                              float amplitude = 0.5f)
    {
        jassert (buffer.getNumChannels() >= 2);

        const auto numSamples = buffer.getNumSamples();

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto leftPhase = juce::MathConstants<double>::twoPi * leftFrequencyHz * static_cast<double> (sample) / sampleRate;
            const auto rightPhase = juce::MathConstants<double>::twoPi * rightFrequencyHz * static_cast<double> (sample) / sampleRate;

            left[sample] = amplitude * static_cast<float> (std::sin (leftPhase));
            right[sample] = amplitude * static_cast<float> (std::sin (rightPhase));
        }
    }

    // Root-mean-square level across all channels/samples in the buffer.
    inline double rms (const juce::AudioBuffer<float>& buffer)
    {
        double sumOfSquares = 0.0;
        juce::int64 numValues = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = static_cast<double> (data[sample]);
                sumOfSquares += value * value;
                ++numValues;
            }
        }

        return numValues > 0 ? std::sqrt (sumOfSquares / static_cast<double> (numValues)) : 0.0;
    }

    // Largest absolute sample value across all channels/samples.
    inline float peakAbsolute (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                peak = std::max (peak, std::abs (data[sample]));
        }

        return peak;
    }

    // Returns true if every sample in the buffer is finite (no NaN/Inf).
    inline bool allSamplesFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite (data[sample]))
                    return false;
        }

        return true;
    }
}
