#include "PluginProcessor.h"
#include "dsp/FirmamentEngine.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("getLatencySamples() is always 0 - Firmament has no oversampling or FIR stage", "[latency]")
{
    FirmamentAudioProcessor processor;

    // Before prepareToPlay, JUCE's default AudioProcessor latency is 0.
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (48000.0, 512);
    CHECK (processor.getLatencySamples() == 0);

    // Cross-check the engine reports the same (trivial, but documents the
    // contract explicitly rather than relying on AudioProcessor's default).
    CHECK (FirmamentEngine::getLatencySamples() == 0);
}

TEST_CASE ("Latency stays 0 across repeated prepareToPlay calls and sample-rate changes", "[latency]")
{
    FirmamentAudioProcessor processor;

    processor.prepareToPlay (44100.0, 256);
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (44100.0, 256);
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (96000.0, 512);
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (192000.0, 32);
    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Latency stays 0 regardless of bass-mono crossover setting", "[latency]")
{
    FirmamentAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* bassMonoParam = processor.apvts.getParameter ("bassMonoFreq");
    REQUIRE (bassMonoParam != nullptr);

    bassMonoParam->setValueNotifyingHost (bassMonoParam->convertTo0to1 (250.0f));
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.getLatencySamples() == 0);
}
