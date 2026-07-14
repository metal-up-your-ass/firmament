#include "ParameterLayout.h"
#include "ParameterIds.h"

namespace frmm
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        //======================================================================
        // Width: Mid/Side width scale, 0-200%, default 100% (unity, i.e. an
        // unmodified pass-through of the input's stereo image).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::width, 1 },
            "Width",
            juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        //======================================================================
        // Bass Mono Freq: crossover frequency below which the Side channel is
        // forced to mono, 0-500 Hz, default 0 Hz (off). A skew factor below 1
        // gives finer control resolution over the low, most commonly-used
        // part of the range (typical bass-mono corners sit around 80-200 Hz)
        // while still reaching all the way to 500 Hz.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::bassMonoFreq, 1 },
            "Bass Mono Freq",
            juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f, 0.4f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        //======================================================================
        // Output: trim applied after M/S decode back to L/R.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::output, 1 },
            "Output",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        return layout;
    }
}
