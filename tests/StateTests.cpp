#include "PluginProcessor.h"
#include "params/ParameterIds.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("State round-trip preserves non-default values of every parameter", "[state]")
{
    FirmamentAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* widthParam = processor.apvts.getParameter (ParamIDs::width);
    auto* bassMonoParam = processor.apvts.getParameter (ParamIDs::bassMonoFreq);
    auto* outputParam = processor.apvts.getParameter (ParamIDs::output);

    REQUIRE (widthParam != nullptr);
    REQUIRE (bassMonoParam != nullptr);
    REQUIRE (outputParam != nullptr);

    widthParam->setValueNotifyingHost (widthParam->convertTo0to1 (175.0f));
    bassMonoParam->setValueNotifyingHost (bassMonoParam->convertTo0to1 (220.0f));
    outputParam->setValueNotifyingHost (outputParam->convertTo0to1 (-4.5f));

    const auto savedWidth = widthParam->getValue();
    const auto savedBassMono = bassMonoParam->getValue();
    const auto savedOutput = outputParam->getValue();

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Reset every parameter back to its default before restoring, so the
    // round-trip assertion below can't pass by accident.
    widthParam->setValueNotifyingHost (widthParam->getDefaultValue());
    bassMonoParam->setValueNotifyingHost (bassMonoParam->getDefaultValue());
    outputParam->setValueNotifyingHost (outputParam->getDefaultValue());

    REQUIRE (widthParam->getValue() != Catch::Approx (savedWidth));
    REQUIRE (bassMonoParam->getValue() != Catch::Approx (savedBassMono));
    REQUIRE (outputParam->getValue() != Catch::Approx (savedOutput));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (widthParam->getValue() == Catch::Approx (savedWidth).margin (1e-6));
    CHECK (bassMonoParam->getValue() == Catch::Approx (savedBassMono).margin (1e-6));
    CHECK (outputParam->getValue() == Catch::Approx (savedOutput).margin (1e-6));
}
