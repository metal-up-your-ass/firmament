#pragma once

#include <juce_dsp/juce_dsp.h>

#include "MidSideCodec.h"

// The complete Firmament signal path, independent of juce::AudioProcessor so
// it can be exercised directly by unit tests without instantiating a full
// plugin (see tests/EngineTests.cpp). Owns all DSP state; every filter is
// allocated in prepare() and never reallocated on the audio thread.
//
// Signal flow (see docs/architecture.md for the full diagram):
//
//   L/R -> encode M/S -> scale Side by Width -> [optional] bass-mono
//       crossover on Side (forces Side to zero below BassMonoFreq)
//       -> decode M/S -> L/R -> Output trim
//
// The bass-mono stage uses a single-channel juce::dsp::LinkwitzRileyFilter
// (TPT/topology-preserving-transform structure) applied to the one derived
// Side stream: its dual-output processSample() overload returns matched
// low-pass and high-pass bands whose sum reconstructs the input exactly
// (flat magnitude sum), so discarding the low band and keeping only the high
// band is equivalent to "the Side channel is exactly zero below the
// crossover frequency" - which is what forces the low end to mono, since
// with Side == 0 the M/S decode collapses to L == R == Mid down there. This
// filter structure adds no reported/oversampling-style latency (unlike an
// FIR crossover), so Firmament's total latency is always 0 samples.
class FirmamentEngine
{
public:
    FirmamentEngine();

    // Allocates all DSP state. Must be called (and completed) before the
    // first process() call, and again whenever sample rate/block size/
    // channel count change. `spec.numChannels` is expected to be 2 (stereo);
    // process() is a safe no-op for any other channel count.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears all filter/gain state without deallocating. Safe to call from
    // the audio thread (e.g. on playback stop/loop).
    void reset();

    // Processes `block` in place. `block` must be a 2-channel (stereo)
    // block, channel 0 = left, channel 1 = right, with at most the maximum
    // sample count declared to prepare(); a zero-sample or non-stereo block
    // is a safe no-op. No allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Parameter setters, in real units. Safe to call every block from the
    // audio thread - no allocation/locks.
    void setWidthPercent (float newWidthPercent);
    void setBassMonoFrequencyHz (float newFrequencyHz);
    void setOutputDb (float newOutputDb);

    // Firmament never oversamples or uses any FIR/linear-phase element, so
    // it never has any latency to report.
    static constexpr int getLatencySamples() noexcept { return 0; }

private:
    static constexpr double smoothingTimeSeconds = 0.05;

    double sampleRate = 44100.0;

    // Operates on a single channel (the one derived Side stream), not the
    // stereo bus - prepared with numChannels == 1 regardless of the block's
    // own channel count.
    juce::dsp::LinkwitzRileyFilter<float> bassMonoCrossover;
    juce::dsp::Gain<float> outputGain;

    // Width is a plain multiplicative scale on the Side channel, cheap
    // enough to interpolate per-sample directly (no trig/coefficient
    // recompute involved), so it uses per-sample getNextValue() rather than
    // the once-per-block skip() pattern used for the crossover frequency
    // below.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;

    // Frequency is smoothed multiplicatively (appropriate for a quantity
    // perceived logarithmically) but only ever driven while bass-mono is
    // enabled (lastBassMonoHz > 0); recomputing LinkwitzRileyFilter
    // coefficients involves a tan() call, so - like Overture's Tight/Tone
    // filters - this is re-derived once per block rather than per sample.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> bassMonoFrequencySmoothed;

    // Last commanded values, re-applied to the smoothers on every prepare()
    // so re-preparing (sample-rate change, etc.) never resets a live
    // parameter back to a default or lets a smoother start from an invalid
    // value. 0 Hz is the frozen "bass-mono off" sentinel (see ParameterIds.h)
    // and is deliberately never fed to the smoother/filter, which requires a
    // strictly positive, sub-Nyquist cutoff.
    float lastWidthPercent = 100.0f;
    float lastBassMonoHz = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmamentEngine)
};
