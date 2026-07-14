#include "dsp/FirmamentEngine.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

// Firmament is a purely linear signal path (Mid/Side scale + a zero-latency
// crossover + an output trim) with no saturating/nonlinear stage anywhere in
// it, unlike e.g. Overture's clipper. That means it has no built-in ceiling:
// a single pass at maximum Width (200%) and maximum Output (+24 dB) can and
// should produce a louder-than-unity result for full-scale input - that is
// the parameter doing exactly what it is documented to do, not a bug.
// These tests document the actual, bounded behaviour of a single realistic
// processBlock() call (a host always supplies a fresh block of audio on
// every callback, never re-feeds a plugin's own prior output back in as new
// input), as a sane-headroom regression test distinct from the NaN/Inf
// sweep in RobustnessTests.cpp.
TEST_CASE ("Single-pass full-scale input at maximum Width/Output stays within sane, finite headroom", "[dsp][engine][gainstaging]")
{
    FirmamentEngine engine;
    engine.setWidthPercent (200.0f);
    engine.setBassMonoFrequencyHz (500.0f);
    engine.setOutputDb (24.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    engine.prepare (spec);

    for (int block = 0; block < 8; ++block)
    {
        // A *fresh* full-scale signal every block, as a real host would
        // supply - not the previous block's already-amplified output.
        juce::AudioBuffer<float> buffer (2, 512);
        TestHelpers::fillStereoWithDistinctSines (buffer, 48000.0, 1000.0, 1300.0, 1.0f);

        juce::dsp::AudioBlock<float> audioBlock (buffer);
        engine.process (audioBlock);

        CHECK (TestHelpers::allSamplesFinite (buffer));

        // Worst case: Side scaled to 2x plus a fully-additive Mid/Side
        // decode (up to 2x the encoded amplitude) at +24 dB (~15.85x) of a
        // 1.0-amplitude input - comfortably under 100 with margin for the
        // two overlapping test tones' constructive interference.
        CHECK (TestHelpers::peakAbsolute (buffer) < 100.0f);
    }
}
