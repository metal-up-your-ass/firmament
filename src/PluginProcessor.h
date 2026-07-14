#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/FirmamentEngine.h"

// Firmament: a stereo widener/imager built around Mid/Side encode/decode.
// Signal flow lives in FirmamentEngine (src/dsp) so it stays unit-testable
// independent of this AudioProcessor; this class is just APVTS + host
// plumbing around it. Firmament fundamentally needs a stereo signal to
// operate on (Mid/Side encoding requires both L and R) - see
// isBusesLayoutSupported() and processBlock() for how a mono input bus is
// handled gracefully rather than rejected outright.
class FirmamentAudioProcessor final : public juce::AudioProcessor
{
public:
    FirmamentAudioProcessor();
    ~FirmamentAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    FirmamentEngine engine;

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* widthPercent = nullptr;
    std::atomic<float>* bassMonoFreqHz = nullptr;
    std::atomic<float>* outputDb = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirmamentAudioProcessor)
};
