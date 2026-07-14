#include "FirmamentEngine.h"

#include <cmath>

namespace
{
    // Keeps a requested bass-mono crossover frequency strictly below Nyquist
    // and strictly positive, as juce::dsp::LinkwitzRileyFilter::
    // setCutoffFrequency() requires (JUCE 8.0.14,
    // juce_dsp/processors/juce_LinkwitzRileyFilter.cpp asserts
    // isPositiveAndBelow(cutoff, sampleRate * 0.5)). 500 Hz (the parameter's
    // documented maximum) is far below Nyquist at any realistic audio sample
    // rate, but this guard costs nothing and removes the assumption entirely.
    float clampBelowNyquist (float frequencyHz, double sampleRate) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRate) * 0.5f;
        const auto maxHz = juce::jmax (5.0f, nyquist * 0.9f);
        return juce::jlimit (5.0f, maxHz, frequencyHz);
    }
}

FirmamentEngine::FirmamentEngine() = default;

void FirmamentEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // The bass-mono crossover runs on a single derived stream (the Side
    // channel), independent of however many channels the host bus has.
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    bassMonoCrossover.prepare (monoSpec);

    outputGain.setRampDurationSeconds (smoothingTimeSeconds);
    outputGain.prepare (spec);

    widthSmoothed.reset (sampleRate, smoothingTimeSeconds);
    widthSmoothed.setCurrentAndTargetValue (lastWidthPercent * 0.01f);

    // Seed the frequency smoother with a safe, strictly-positive value even
    // while bass-mono is off, so that if it is engaged later mid-stream the
    // smoother starts from a sane point rather than 0 Hz (an invalid filter
    // cutoff). This has no audible effect while disabled, since process()
    // never calls setCutoffFrequency()/runs the crossover unless
    // lastBassMonoHz > 0.
    const auto seedFrequencyHz = lastBassMonoHz > 0.0f ? juce::jmax (5.0f, lastBassMonoHz) : 20.0f;
    bassMonoFrequencySmoothed.reset (sampleRate, smoothingTimeSeconds);
    bassMonoFrequencySmoothed.setCurrentAndTargetValue (clampBelowNyquist (seedFrequencyHz, sampleRate));

    reset();

    // Prime the filter coefficients immediately so the very first process()
    // call runs with correct, non-default coefficients rather than an
    // identity/uninitialised state, in case bass-mono is already engaged
    // when prepare() runs.
    bassMonoCrossover.setCutoffFrequency (clampBelowNyquist (seedFrequencyHz, sampleRate));
}

void FirmamentEngine::reset()
{
    bassMonoCrossover.reset();
    outputGain.reset();
}

void FirmamentEngine::setWidthPercent (float newWidthPercent)
{
    lastWidthPercent = newWidthPercent;
    widthSmoothed.setTargetValue (newWidthPercent * 0.01f);
}

void FirmamentEngine::setBassMonoFrequencyHz (float newFrequencyHz)
{
    lastBassMonoHz = newFrequencyHz;

    // 0 Hz (off) is deliberately never forwarded to the smoother/filter -
    // process() gates the whole crossover stage on lastBassMonoHz > 0.
    if (newFrequencyHz > 0.0f)
        bassMonoFrequencySmoothed.setTargetValue (juce::jmax (5.0f, newFrequencyHz));
}

void FirmamentEngine::setOutputDb (float newOutputDb)
{
    outputGain.setGainDecibels (newOutputDb);
}

void FirmamentEngine::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0)
        return;

    // The M/S encode/decode below is only meaningful for a genuine stereo
    // pair; the processor guarantees this (duplicating mono input to both
    // channels before calling process(), see PluginProcessor.cpp), but this
    // guard makes the engine safe to call standalone (e.g. from a test) with
    // any other channel count too.
    if (block.getNumChannels() != 2)
        return;

    const bool bassMonoEnabled = lastBassMonoHz > 0.0f;

    // Coefficient recomputation involves a tan() call, so - like Overture's
    // Tight/Tone filters - the crossover frequency is smoothed and re-derived
    // once per block rather than per sample.
    if (bassMonoEnabled)
    {
        const auto freqHz = clampBelowNyquist (bassMonoFrequencySmoothed.skip (static_cast<int> (numSamples)), sampleRate);
        bassMonoCrossover.setCutoffFrequency (freqHz);
    }

    auto* left = block.getChannelPointer (0);
    auto* right = block.getChannelPointer (1);

    for (size_t i = 0; i < numSamples; ++i)
    {
        // Width is a plain multiplicative scale with no coefficients to
        // recompute, so it is cheap enough to interpolate sample-accurately.
        const auto widthProportion = widthSmoothed.getNextValue();

        // A single NaN/Inf input sample must never be allowed to reach the
        // bass-mono crossover: unlike a feedforward gain stage, an IIR
        // filter's internal state (s1-s4) carries a poisoned value forward
        // indefinitely once it gets in (NaN propagates through any further
        // multiply/add), permanently corrupting every subsequent block even
        // once clean input resumes. Scrubbing here - cheap, branch-only, no
        // allocation - keeps that failure mode local to the single bad
        // sample instead of the rest of the stream.
        const auto leftSample = std::isfinite (left[i]) ? left[i] : 0.0f;
        const auto rightSample = std::isfinite (right[i]) ? right[i] : 0.0f;

        const auto encoded = MidSideCodec::encode (leftSample, rightSample);
        auto side = encoded.side * widthProportion;

        if (bassMonoEnabled)
        {
            // Discarding the low band and keeping only the high band is
            // exactly "Side == 0 below the crossover frequency" (the two
            // bands sum to the input, by construction of the Linkwitz-Riley
            // TPT structure), which is what forces the low end to mono once
            // decoded back to L/R below.
            float lowBand = 0.0f;
            float highBand = 0.0f;
            bassMonoCrossover.processSample (0, side, lowBand, highBand);
            side = highBand;
        }

        const auto decoded = MidSideCodec::decode (encoded.mid, side);
        left[i] = decoded.left;
        right[i] = decoded.right;
    }

    juce::dsp::ProcessContextReplacing<float> context (block);
    outputGain.process (context);
}
