#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// StageDeck colour scheme
namespace Pal
{
    const juce::Colour bgTop { 0xff22303f }, bgBottom { 0xff111a23 }, panel { 0xff0e161f }, panelEdge { 0xff2a3a4b },
        text { 0xffe9eef3 }, dim { 0xff8394a6 }, faint { 0xff3b4d60 }, blue { 0xff5ab4e8 }, gold { 0xfff2c14e },
        knobTop { 0xff3c4c5e }, knobBottom { 0xff17212b }, badge { 0xff0d151d },
        green { 0xff5fd08a }, red { 0xffe8615a };
}

juce::Font uiFont (float h, bool bold = false);

//==============================================================================
class ProRLook : public juce::LookAndFeel_V4
{
public:
    ProRLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    float uiScale = 1.0f;
};

//==============================================================================
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    Knob (SafeDeckProcessor&, const juce::String& paramId, const juce::String& title,
          const juce::String& lo, const juce::String& hi, bool big = false);
    ~Knob() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent&) override { repaint(); }

    juce::Slider slider;
private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String title, lo, hi;
    bool big;
};

//==============================================================================
class SafeDeckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SafeDeckEditor (SafeDeckProcessor&);
    ~SafeDeckEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawStatus (juce::Graphics&, juce::Rectangle<float>);
    void drawMeters (juce::Graphics&, juce::Rectangle<float>);
    void drawLog (juce::Graphics&, juce::Rectangle<float>);
    void drawPanel (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);

    SafeDeckProcessor& proc;
    ProRLook look;

    Knob ceiling, release, panic;
    juce::ToggleButton limiterToggle { "Limiter" }, truePeakToggle { "True peak" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAtt, truePeakAtt;
    juce::TextButton clearButton { "CLEAR" };
    juce::TooltipWindow tooltipWindow { this, 700 };

    juce::Rectangle<int> headerArea, statusArea, meterArea, controlArea, logArea;
    float sc = 1.0f;

    // meter display state (dB)
    float inDb = -100.0f, outDb = -100.0f, grDb = 0.0f, inHold = -100.0f, outHold = -100.0f, grHold = 0.0f;
    int inHoldAge = 0, outHoldAge = 0, grHoldAge = 0;
    int state = 0;
    juce::uint32 lastEventCount = 0;
    float flash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SafeDeckEditor)
};
