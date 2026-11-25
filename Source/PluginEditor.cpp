#include "PluginEditor.h"

using namespace juce;

Font uiFont (float h, bool bold)
{
    auto o = FontOptions (h);
    return Font (bold ? o.withStyle ("Bold") : o);
}

//==============================================================================
ProRLook::ProRLook()
{
    setColour (TextButton::textColourOffId, Pal::text);
    setColour (ToggleButton::textColourId, Pal::dim);
    setColour (TooltipWindow::backgroundColourId, Pal::panel);
    setColour (TooltipWindow::textColourId, Pal::text);
    setColour (TooltipWindow::outlineColourId, Pal::panelEdge);
}

void ProRLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const bool big = (bool) s.getProperties()["big"];
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f * 0.78f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);

    g.setColour (Pal::faint);
    const int ticks = big ? 21 : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = start + (float) i / (float) (ticks - 1) * (end - start);
        const auto p = c.getPointOnCircumference (r * 1.2f, a);
        g.fillEllipse (p.x - 1.2f * uiScale, p.y - 1.2f * uiScale, 2.4f * uiScale, 2.4f * uiScale);
    }

    {
        Path arc;
        arc.addCentredArc (c.x, c.y, r * 1.08f, r * 1.08f, 0.0f, start, angle, true);
        g.setColour (Pal::blue.withAlpha (s.isMouseOverOrDragging() ? 1.0f : 0.75f));
        g.strokePath (arc, PathStrokeType (2.0f * uiScale, PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r, c.y - r + 2.0f * uiScale, r * 2, r * 2);
    ColourGradient grad (Pal::knobTop, c.x, c.y - r, Pal::knobBottom, c.x, c.y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawEllipse (c.x - r + 0.5f, c.y - r + 0.5f, r * 2 - 1, r * 2 - 1, 1.0f);
    if (big)
    {
        const float ri = r * 0.82f;
        g.setColour (Colour (0xff131c26));
        g.fillEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2);
        g.setColour (Pal::text);
        g.setFont (uiFont (r * 0.3f, true));
        g.drawText (s.getTextFromValue (s.getValue()), Rectangle<float> (c.x - ri, c.y - r * 0.2f, ri * 2, r * 0.4f), Justification::centred);
    }

    const auto p1 = c.getPointOnCircumference (big ? r * 0.86f : r * 0.3f, angle);
    const auto p2 = c.getPointOnCircumference (r * 0.95f, angle);
    g.setColour (Colours::white);
    g.drawLine ({ p1, p2 }, (big ? 3.0f : 2.4f) * uiScale);
}

void ProRLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? Pal::blue.withAlpha (0.25f) : (over ? Colour (0xff1a2633) : Pal::panel));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (over ? Pal::blue.withAlpha (0.7f) : Pal::panelEdge);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
}

void ProRLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (Pal::text);
    g.setFont (uiFont (12.0f * uiScale, true));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

void ProRLook::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float h = 14.0f * uiScale, w = 26.0f * uiScale;
    auto sw = Rectangle<float> (r.getX(), r.getCentreY() - h * 0.5f, w, h);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::blue.withAlpha (0.85f) : Pal::faint);
    g.fillRoundedRectangle (sw, h * 0.5f);
    const float d = h - 4.0f;
    g.setColour (Colours::white);
    g.fillEllipse (on ? sw.getRight() - d - 2.0f : sw.getX() + 2.0f, sw.getY() + 2.0f, d, d);
    g.setColour (over ? Pal::text : Pal::dim);
    g.setFont (uiFont (12.0f * uiScale, true));
    g.drawText (b.getButtonText().toUpperCase(), r.withTrimmedLeft (w + 8.0f * uiScale), Justification::centredLeft);
}

//==============================================================================
Knob::Knob (SafeDeckProcessor& p, const String& paramId, const String& t, const String& l, const String& h, bool isBig)
    : title (t), lo (l), hi (h), big (isBig)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.getProperties().set ("big", big);
    slider.setMouseDragSensitivity (big ? 260 : 200);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    if (auto* param = p.apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.addListener (this);
    slider.addMouseListener (this, false);
}

Knob::~Knob() { slider.removeListener (this); }

void Knob::resized()
{
    auto b = getLocalBounds();
    b.removeFromBottom (jmax (14, getHeight() / 6));
    slider.setBounds (b);
}

void Knob::paint (Graphics& g)
{
    const int labelH = jmax (14, getHeight() / 6);
    auto lab = getLocalBounds().removeFromBottom (labelH).toFloat();
    const float fh = labelH * 0.72f;
    const bool active = slider.isMouseOverOrDragging() && ! big;
    g.setColour (active ? Pal::blue : Pal::text);
    g.setFont (uiFont (fh, true));
    g.drawText (active ? slider.getTextFromValue (slider.getValue()) : title, lab, Justification::centred);

    auto sb = slider.getBounds().toFloat();
    const float r = jmin (sb.getWidth(), sb.getHeight()) * 0.5f * 0.78f;
    const auto c = sb.getCentre();
    g.setColour (Pal::dim);
    g.setFont (uiFont (fh * 0.72f, true));
    const float yy = c.y + r;
    g.drawText (lo, Rectangle<float> (c.x - r * 2.2f, yy, r * 1.6f, fh), Justification::centredRight);
    g.drawText (hi, Rectangle<float> (c.x + r * 0.6f, yy, r * 1.6f, fh), Justification::centredLeft);
}

//==============================================================================
SafeDeckEditor::SafeDeckEditor (SafeDeckProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      ceiling (p, ParamIDs::ceiling, "CEILING", "-12", "0", true),
      release (p, ParamIDs::release, "RELEASE", "fast", "slow"),
      panic (p, ParamIDs::panic, "PANIC LEVEL", "0", "+24")
{
    setLookAndFeel (&look);
    for (auto* c : { (Component*) &ceiling, (Component*) &release, (Component*) &panic,
                     (Component*) &limiterToggle, (Component*) &truePeakToggle, (Component*) &clearButton })
        addAndMakeVisible (c);

    limiterAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::limiter, limiterToggle);
    truePeakAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::truePeak, truePeakToggle);

    ceiling.slider.setTooltip ("The limiter never lets the output go above this level");
    release.slider.setTooltip ("How fast the limiter lets go after a peak");
    panic.slider.setTooltip ("Anything louder than this coming IN is treated as a blast: the output mutes, then fades back in");
    truePeakToggle.setTooltip ("Also catches the peaks between samples that appear after conversion to analog");
    limiterToggle.setTooltip ("Off: only the safety guard is active (NaN and panic level), the sound passes untouched");
    clearButton.setTooltip ("Clear the event log");
    clearButton.onClick = [this] { proc.clearedAt = proc.engine.eventCount.load(); repaint(); };

    lastEventCount = proc.engine.eventCount.load();

    // read the remembered size first: setResizeLimits() triggers resized(), which stores the size
    const int savedW = jlimit (600, 1800, proc.editorWidth), savedH = jlimit (350, 1050, proc.editorHeight);
    setResizable (true, true);
    setResizeLimits (600, 350, 1800, 1050);
    getConstrainer()->setFixedAspectRatio (720.0 / 420.0);
    setSize (savedW, savedH);
    startTimerHz (30);
}

SafeDeckEditor::~SafeDeckEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SafeDeckEditor::timerCallback()
{
    auto toDb = [] (float v) { return v > 1.0e-6f ? 20.0f * std::log10 (v) : -120.0f; };
    const float in = toDb (proc.engine.takeInPeak());
    const float out = toDb (proc.engine.takeOutPeak());
    const float gr = toDb (proc.engine.takeMinGain());   // <= 0

    const float fall = 1.2f;   // dB per frame
    inDb = jmax (in, inDb - fall);
    outDb = jmax (out, outDb - fall);
    grDb = jmin (gr, grDb + fall * 0.5f);
    auto hold = [] (float v, float& h, int& age, bool up)
    {
        if (up ? v >= h : v <= h) { h = v; age = 0; }
        else if (++age > 45) h = v;
    };
    hold (inDb, inHold, inHoldAge, true);
    hold (outDb, outHold, outHoldAge, true);
    hold (grDb, grHold, grHoldAge, false);

    state = proc.engine.getState();
    const auto ec = proc.engine.eventCount.load();
    if (ec != lastEventCount) { lastEventCount = ec; flash = 1.0f; }
    flash = jmax (0.0f, flash - 0.04f);
    repaint();
}

void SafeDeckEditor::drawPanel (Graphics& g, Rectangle<float> r, const String& title)
{
    g.setColour (Pal::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (r, 8 * sc);
    g.setColour (Pal::panelEdge);
    g.drawRoundedRectangle (r, 8 * sc, 1.0f);
    g.setColour (Pal::dim);
    g.setFont (uiFont (10.5f * sc, true));
    g.drawText (title, r.reduced (12 * sc, 8 * sc).removeFromTop (14 * sc), Justification::centredLeft);
}

void SafeDeckEditor::drawStatus (Graphics& g, Rectangle<float> r)
{
    drawPanel (g, r, "STATUS");
    auto a = r.reduced (12 * sc, 8 * sc);
    a.removeFromTop (18 * sc);

    Colour col = Pal::green; String big = "ALL CLEAR", small = "Signal passes untouched";
    if (state == safe::stFadeOut || state == safe::stMuted) { col = Pal::red; big = "MUTED"; small = "Bad signal coming in"; }
    else if (state == safe::stFadeIn) { col = Pal::gold; big = "FADING IN"; small = "Signal is clean again"; }
    else if (grHold < -0.05f) { col = Pal::blue; big = "LIMITING"; small = "Peaks held at the ceiling"; }

    // lamp
    const float d = jmin (a.getWidth() * 0.42f, a.getHeight() * 0.48f);
    auto lamp = Rectangle<float> (d, d).withCentre ({ a.getCentreX(), a.getY() + d * 0.5f + 6 * sc });
    for (int i = 3; i > 0; --i)
    {
        g.setColour (col.withAlpha (0.06f * (float) i + 0.15f * flash));
        g.fillEllipse (lamp.expanded ((float) i * 5.0f * sc));
    }
    g.setGradientFill (ColourGradient (col.brighter (0.4f), lamp.getCentreX(), lamp.getY(), col.darker (0.6f), lamp.getCentreX(), lamp.getBottom(), false));
    g.fillEllipse (lamp);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawEllipse (lamp.reduced (1.0f), 1.0f);

    auto t = a.withTrimmedTop (lamp.getBottom() - a.getY() + 10 * sc);
    g.setColour (Pal::text);
    g.setFont (uiFont (17.0f * sc, true));
    g.drawText (big, t.removeFromTop (22 * sc), Justification::centred);
    g.setColour (Pal::dim);
    g.setFont (uiFont (11.5f * sc, true));
    g.drawText (small, t.removeFromTop (16 * sc), Justification::centred);
    t.removeFromTop (6 * sc);
    g.setFont (uiFont (10.5f * sc, true));
    g.drawText ("NaN/inf samples: " + String (proc.engine.nanTotal.load()) + "    Over panic: " + String (proc.engine.overTotal.load()),
                t.removeFromTop (14 * sc), Justification::centred);
}

void SafeDeckEditor::drawMeters (Graphics& g, Rectangle<float> r)
{
    drawPanel (g, r, "LEVELS");
    auto a = r.reduced (14 * sc, 8 * sc);
    a.removeFromTop (20 * sc);
    auto labels = a.removeFromBottom (32 * sc);

    const float lo = -48.0f, hi = 6.0f;
    const float ceilDb = proc.apvts.getRawParameterValue (ParamIDs::ceiling)->load();
    const bool limOn = proc.apvts.getRawParameterValue (ParamIDs::limiter)->load() > 0.5f;
    const float barW = a.getWidth() / 5.0f;

    auto drawBar = [&] (Rectangle<float> b, float v, float holdV, bool isGr, const String& name, const String& readout, Rectangle<float> lab)
    {
        g.setColour (Pal::badge);
        g.fillRoundedRectangle (b, 3 * sc);
        if (isGr)
        {
            const float frac = jlimit (0.0f, 1.0f, -v / 24.0f);
            g.setColour (Pal::gold);
            g.fillRoundedRectangle (b.withHeight (b.getHeight() * frac), 3 * sc);
            const float hy = b.getY() + b.getHeight() * jlimit (0.0f, 1.0f, -holdV / 24.0f);
            g.drawHorizontalLine ((int) hy, b.getX(), b.getRight());
        }
        else
        {
            auto y = [&] (float db) { return b.getBottom() - b.getHeight() * jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)); };
            const float top = y (v);
            g.setGradientFill (ColourGradient (Pal::red, 0, y (hi), Pal::blue, 0, y (-12.0f), false));
            g.fillRoundedRectangle (b.withTop (top), 3 * sc);
            g.setColour (holdV > 0.0f ? Pal::red : Pal::text.withAlpha (0.8f));
            g.drawHorizontalLine ((int) y (holdV), b.getX(), b.getRight());
        }
        g.setColour (Pal::text);
        g.setFont (uiFont (10.5f * sc, true));
        g.drawText (name, lab.withHeight (14 * sc), Justification::centred);
        g.setColour (Pal::dim);
        g.drawText (readout, lab.withTrimmedTop (15 * sc).withHeight (14 * sc), Justification::centred);
    };

    auto fmt = [] (float db) { return db <= -100.0f ? String ("-inf") : String (db, 1); };
    auto inB = Rectangle<float> (a.getX() + barW * 0.5f, a.getY(), barW, a.getHeight());
    auto grB = inB.translated (barW * 1.5f, 0);
    auto outB = grB.translated (barW * 1.5f, 0);

    // scale
    g.setFont (uiFont (9.0f * sc));
    for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float yy = inB.getBottom() - inB.getHeight() * (db - lo) / (hi - lo);
        g.setColour (Pal::faint);
        g.drawHorizontalLine ((int) yy, inB.getRight() + 2 * sc, grB.getX() - 2 * sc);
        g.setColour (Pal::dim.withAlpha (0.7f));
        g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), Rectangle<float> (inB.getRight(), yy - 6 * sc, grB.getX() - inB.getRight(), 12 * sc), Justification::centred);
    }

    drawBar (inB, inDb, inHold, false, "IN", fmt (inHold), labels.withX (inB.getX() - barW * 0.25f).withWidth (barW * 1.5f));
    drawBar (grB, grDb, grHold, true, "GR", grHold > -0.05f ? String ("0.0") : String (grHold, 1), labels.withX (grB.getX() - barW * 0.25f).withWidth (barW * 1.5f));
    drawBar (outB, outDb, outHold, false, "OUT", fmt (outHold), labels.withX (outB.getX() - barW * 0.25f).withWidth (barW * 1.5f));

    if (limOn)
    {
        const float yy = outB.getBottom() - outB.getHeight() * jlimit (0.0f, 1.0f, (ceilDb - lo) / (hi - lo));
        g.setColour (Pal::gold);
        g.drawLine (outB.getX() - 3 * sc, yy, outB.getRight() + 3 * sc, yy, 1.5f * sc);
    }
}

void SafeDeckEditor::drawLog (Graphics& g, Rectangle<float> r)
{
    drawPanel (g, r, "EVENT LOG");
    auto a = r.reduced (12 * sc, 8 * sc);
    a.removeFromTop (18 * sc);

    const uint32 count = proc.engine.eventCount.load (std::memory_order_acquire);
    const uint32 from = jmax (proc.clearedAt.load(), count > (uint32) safe::Engine::maxEvents ? count - (uint32) safe::Engine::maxEvents : 0u);
    g.setFont (uiFont (11.5f * sc, true));
    if (count <= from)
    {
        g.setColour (Pal::dim);
        g.drawText ("Nothing happened. Every sample so far was safe.", a, Justification::centredLeft);
        return;
    }

    // newest first, two columns
    const int shown = (int) jmin<uint32> (count - from, 4);
    const float rowH = a.getHeight() / 2.0f, colW = a.getWidth() / 2.0f;
    for (int i = 0; i < shown; ++i)
    {
        const uint32 k = count - 1 - (uint32) i;
        const auto& e = proc.engine.events[(size_t) (k % safe::Engine::maxEvents)];
        const int type = e.type.load();
        const double t = jmax (0.0, e.time.load());
        const int mins = (int) (t / 60.0);
        const String time = String (mins) + ":" + String (t - mins * 60.0, 1).paddedLeft ('0', 4);
        auto row = Rectangle<float> (a.getX() + (float) (i / 2) * colW, a.getY() + (float) (i % 2) * rowH, colW, rowH);
        g.setColour (type == safe::evNaN ? Pal::red : Pal::gold);
        g.fillEllipse (row.getX(), row.getCentreY() - 4 * sc, 8 * sc, 8 * sc);
        g.setColour (i == 0 ? Pal::text : Pal::dim);
        g.drawText (time + "   " + (type == safe::evNaN ? "NaN/inf from upstream - muted"
                                                          : "Blast over panic level - muted"),
                    row.withTrimmedLeft (16 * sc), Justification::centredLeft);
    }
}

void SafeDeckEditor::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Pal::bgTop, 0, 0, Pal::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    auto h = headerArea.toFloat().reduced (16 * sc, 0);
    const auto f = uiFont (22.0f * sc, true);
    const float aw = GlyphArrangement::getStringWidth (f, "Safe");
    g.setFont (f);
    g.setColour (Pal::text);
    g.drawText ("Safe", h, Justification::centredLeft);
    g.setColour (Pal::blue);
    g.drawText ("Deck", h.withTrimmedLeft (aw), Justification::centredLeft);
    const float tx = aw + GlyphArrangement::getStringWidth (f, "Deck") + 14 * sc;
    g.setColour (Pal::dim);
    g.setFont (uiFont (11.0f * sc, true));
    g.drawText ("MASTER SAFETY", h.withTrimmedLeft (tx), Justification::centredLeft);
    g.drawText ("Latency " + String (proc.engine.latency()) + " smp (auto-compensated)", h, Justification::centredRight);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawHorizontalLine (headerArea.getBottom(), 0.0f, (float) getWidth());

    drawStatus (g, statusArea.toFloat());
    drawMeters (g, meterArea.toFloat());
    drawPanel (g, controlArea.toFloat(), "LIMITER & GUARD");
    drawLog (g, logArea.toFloat());
}

void SafeDeckEditor::resized()
{
    proc.editorWidth = getWidth();
    proc.editorHeight = getHeight();
    sc = jmin (getWidth() / 720.0f, getHeight() / 420.0f);
    look.uiScale = sc;

    auto r = getLocalBounds();
    headerArea = r.removeFromTop ((int) (44 * sc));
    r.reduce ((int) (10 * sc), (int) (8 * sc));
    logArea = r.removeFromBottom ((int) (78 * sc));
    r.removeFromBottom ((int) (8 * sc));
    statusArea = r.removeFromLeft ((int) (200 * sc));
    r.removeFromLeft ((int) (8 * sc));
    meterArea = r.removeFromLeft ((int) (170 * sc));
    r.removeFromLeft ((int) (8 * sc));
    controlArea = r;

    auto c = controlArea.reduced ((int) (12 * sc), (int) (8 * sc));
    c.removeFromTop ((int) (18 * sc));
    auto toggles = c.removeFromBottom ((int) (26 * sc));
    limiterToggle.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2));
    truePeakToggle.setBounds (toggles);
    c.removeFromBottom ((int) (6 * sc));
    auto left = c.removeFromLeft (c.getWidth() * 55 / 100);
    ceiling.setBounds (left.reduced ((int) (4 * sc)));
    release.setBounds (c.removeFromTop (c.getHeight() / 2).reduced ((int) (4 * sc), (int) (2 * sc)));
    panic.setBounds (c.reduced ((int) (4 * sc), (int) (2 * sc)));

    clearButton.setBounds (logArea.getRight() - (int) (78 * sc), logArea.getY() + (int) (6 * sc), (int) (66 * sc), (int) (20 * sc));
}
