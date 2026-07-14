#include "dsp/FirmamentEngine.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 2048;

    juce::dsp::ProcessSpec makeTestSpec()
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (testBlockSize);
        spec.numChannels = 2;
        return spec;
    }
}

TEST_CASE ("Width 100% + BassMono off nulls against the input (unity M/S round-trip)", "[dsp][engine][null]")
{
    FirmamentEngine engine;
    engine.setWidthPercent (100.0f);
    engine.setBassMonoFrequencyHz (0.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec();
    engine.prepare (spec);

    REQUIRE (engine.getLatencySamples() == 0);

    juce::AudioBuffer<float> reference (2, testBlockSize);
    // Genuinely distinct L/R content, not a mono source - a real M/S round-
    // trip test has to prove the whole encode/scale/decode chain is
    // transparent, not just that it happens to be transparent for a signal
    // that was already mono (Side == 0) to begin with.
    TestHelpers::fillStereoWithDistinctSines (reference, testSampleRate, 1000.0, 1300.0, 0.5f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);

    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

    // < -90 dBFS residual, in linear amplitude.
    constexpr float tolerance = 3.1623e-5f; // 10^(-90/20)

    for (int channel = 0; channel < reference.getNumChannels(); ++channel)
    {
        const auto* refData = reference.getReadPointer (channel);
        const auto* outData = processed.getReadPointer (channel);

        float maxResidual = 0.0f;

        for (int i = 0; i < testBlockSize; ++i)
            maxResidual = std::max (maxResidual, std::abs (outData[i] - refData[i]));

        CHECK (maxResidual < tolerance);
    }
}

TEST_CASE ("Width 0% collapses to mono (L == R)", "[dsp][engine]")
{
    FirmamentEngine engine;
    engine.setWidthPercent (0.0f);
    engine.setBassMonoFrequencyHz (0.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec();
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillStereoWithDistinctSines (buffer, testSampleRate, 1000.0, 1300.0, 0.6f);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    const auto* left = buffer.getReadPointer (0);
    const auto* right = buffer.getReadPointer (1);

    float maxDifference = 0.0f;

    for (int i = 0; i < testBlockSize; ++i)
        maxDifference = std::max (maxDifference, std::abs (left[i] - right[i]));

    CHECK (maxDifference < 1.0e-6f);
}

TEST_CASE ("Mono downmix (L + R) is unaffected by Width or bass-mono, at any setting", "[dsp][engine]")
{
    // By construction, decode() always returns L = Mid + Side, R = Mid -
    // Side, so L + R == 2 * Mid regardless of what happens to Side - this is
    // the defining mono-compatibility property of M/S widening: increasing
    // apparent stereo width never changes what a mono listener (or a mono
    // fold-down) hears. This test exercises that invariant end-to-end
    // through the engine, across a spread of Width and bass-mono settings.
    struct Setting
    {
        float widthPercent;
        float bassMonoHz;
    };

    const Setting settings[] = {
        { 0.0f, 0.0f },
        { 100.0f, 0.0f },
        { 150.0f, 0.0f },
        { 200.0f, 0.0f },
        { 200.0f, 150.0f },
    };

    juce::AudioBuffer<float> reference (2, testBlockSize);
    TestHelpers::fillStereoWithDistinctSines (reference, testSampleRate, 1000.0, 1300.0, 0.4f);

    juce::AudioBuffer<float> referenceMonoSum (1, testBlockSize);
    {
        const auto* left = reference.getReadPointer (0);
        const auto* right = reference.getReadPointer (1);
        auto* sum = referenceMonoSum.getWritePointer (0);

        for (int i = 0; i < testBlockSize; ++i)
            sum[i] = left[i] + right[i];
    }

    for (const auto& setting : settings)
    {
        FirmamentEngine engine;
        engine.setWidthPercent (setting.widthPercent);
        engine.setBassMonoFrequencyHz (setting.bassMonoHz);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec();
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (reference);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        const auto* left = processed.getReadPointer (0);
        const auto* right = processed.getReadPointer (1);
        const auto* expectedSum = referenceMonoSum.getReadPointer (0);

        float maxResidual = 0.0f;

        for (int i = 0; i < testBlockSize; ++i)
            maxResidual = std::max (maxResidual, std::abs ((left[i] + right[i]) - expectedSum[i]));

        CHECK (maxResidual < 1.0e-4f);
    }
}

TEST_CASE ("Bass-mono forces the Side channel to (near) zero below the crossover frequency", "[dsp][engine][bassmono]")
{
    FirmamentEngine engine;
    engine.setWidthPercent (200.0f); // maximally wide, to make any residual Side content obvious
    engine.setBassMonoFrequencyHz (300.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec();
    engine.prepare (spec);

    // A low-frequency test tone (well below the 300 Hz crossover) with
    // genuine stereo content: without bass-mono this would stay wide, with
    // it engaged the low end must collapse to mono (L == R).
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillStereoWithDistinctSines (buffer, testSampleRate, 80.0, 90.0, 0.5f);

    juce::dsp::AudioBlock<float> block (buffer);
    // One warm-up block to let the crossover's TPT state settle out of its
    // zero-state turn-on transient before measuring.
    engine.process (block);
    TestHelpers::fillStereoWithDistinctSines (buffer, testSampleRate, 80.0, 90.0, 0.5f);
    engine.process (block);

    const auto* left = buffer.getReadPointer (0);
    const auto* right = buffer.getReadPointer (1);

    // Measure over the settled tail of the block, in dB relative to the
    // 0.5-amplitude input, well clear of the crossover's own passband edge.
    constexpr int measureFrom = testBlockSize / 2;

    float maxDifference = 0.0f;

    for (int i = measureFrom; i < testBlockSize; ++i)
        maxDifference = std::max (maxDifference, std::abs (left[i] - right[i]));

    // -30 dB relative to the 0.5 amplitude input is a generous bound that
    // still clearly distinguishes "forced mono" from "left wide" (which
    // would show a difference on the order of the input amplitude itself).
    CHECK (maxDifference < 0.5f * 0.0316f);
}

TEST_CASE ("Engine reset() clears crossover/gain state without crashing", "[dsp][engine]")
{
    FirmamentEngine engine;
    engine.setWidthPercent (150.0f);
    engine.setBassMonoFrequencyHz (200.0f);
    engine.setOutputDb (6.0f);

    const auto spec = makeTestSpec();
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillStereoWithDistinctSines (buffer, testSampleRate, 1000.0, 1300.0, 0.9f);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    CHECK_NOTHROW (engine.reset());
    CHECK (TestHelpers::allSamplesFinite (buffer));

    TestHelpers::fillStereoWithDistinctSines (buffer, testSampleRate, 1000.0, 1300.0, 0.9f);
    CHECK_NOTHROW (engine.process (block));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}
