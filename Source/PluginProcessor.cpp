#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout SafeDeckProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto db = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };
    auto ms = [] (float v, int) { return String (roundToInt (v)) + " ms"; };

    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::limiter, 1 }, "Limiter", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::ceiling, 1 }, "Ceiling",
                                                  NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::release, 1 }, "Release",
                                                  NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 100.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::truePeak, 1 }, "True Peak", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::panic, 1 }, "Panic Level",
                                                  NormalisableRange<float> (0.0f, 24.0f, 0.5f), 18.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (db)));
    return l;
}

SafeDeckProcessor::SafeDeckProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SAFEDECK", createLayout())
{
    pLimiter = apvts.getRawParameterValue (ParamIDs::limiter);
    pCeiling = apvts.getRawParameterValue (ParamIDs::ceiling);
    pRelease = apvts.getRawParameterValue (ParamIDs::release);
    pTruePeak = apvts.getRawParameterValue (ParamIDs::truePeak);
    pPanic = apvts.getRawParameterValue (ParamIDs::panic);
    engine.prepare (48000.0, 2);
    setLatencySamples (engine.latency());
}

bool SafeDeckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void SafeDeckProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, jmax (1, getTotalNumOutputChannels()));
    engine.setParams (readParams());
    sampleRateForGui = sampleRate;
    setLatencySamples (engine.latency());
}

safe::Params SafeDeckProcessor::readParams() const
{
    safe::Params p;
    p.limiterOn = pLimiter->load() > 0.5f;
    p.ceilingDb = pCeiling->load();
    p.releaseMs = pRelease->load();
    p.truePeak = pTruePeak->load() > 0.5f;
    p.panicDb = pPanic->load();
    return p;
}

void SafeDeckProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = jmin (buffer.getNumChannels(), jmax (getTotalNumInputChannels(), 1));
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n <= 0 || ch <= 0) return;

    // host time for the event log (falls back to time since the plugin started)
    double t = fallbackTime;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto s = pos->getTimeInSeconds()) t = *s;
    fallbackTime += n / jmax (1.0, getSampleRate());

    engine.setParams (readParams());
    engine.process (buffer.getArrayOfWritePointers(), ch, n, t);
}

void SafeDeckProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const int ch = jmin (buffer.getNumChannels(), jmax (getTotalNumInputChannels(), 1));
    if (n > 0 && ch > 0) engine.processBypassed (buffer.getArrayOfWritePointers(), ch, n);
}

void SafeDeckProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("editorW", editorWidth, nullptr);
    state.setProperty ("editorH", editorHeight, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SafeDeckProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            editorWidth = (int) state.getProperty ("editorW", editorWidth);
            editorHeight = (int) state.getProperty ("editorH", editorHeight);
            apvts.replaceState (state);
        }
}

AudioProcessorEditor* SafeDeckProcessor::createEditor() { return new SafeDeckEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SafeDeckProcessor(); }
