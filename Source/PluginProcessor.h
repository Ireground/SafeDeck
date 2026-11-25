#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SafeEngine.h"

namespace ParamIDs
{
    inline const juce::String limiter { "limiter" }, ceiling { "ceiling" }, release { "release" },
                              truePeak { "truepeak" }, panic { "panic" };
}

class SafeDeckProcessor : public juce::AudioProcessor
{
public:
    SafeDeckProcessor();
    ~SafeDeckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SafeDeck"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    safe::Engine engine;
    std::atomic<double> sampleRateForGui { 48000.0 };
    std::atomic<uint32_t> clearedAt { 0 };   // event log: entries before this are hidden

    int editorWidth = 720, editorHeight = 420;

private:
    safe::Params readParams() const;
    std::atomic<float>* pLimiter = nullptr; std::atomic<float>* pCeiling = nullptr; std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pTruePeak = nullptr; std::atomic<float>* pPanic = nullptr;
    double fallbackTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SafeDeckProcessor)
};
