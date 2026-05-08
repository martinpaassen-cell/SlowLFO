#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ── Kingfisher colour palette ─────────────────────────────────────────────
namespace KC {
    const juce::Colour panelBg = juce::Colour(0xff071820u);
    const juce::Colour modBg = juce::Colour(0xff0b2232u);
    const juce::Colour border = juce::Colour(0xff1a4555u);
    const juce::Colour text = juce::Colour(0xffd8eff5u);
    const juce::Colour textDim = juce::Colour(0xff4a7a8au);
    const juce::Colour orange = juce::Colour(0xffe85010u);
    const juce::Colour teal = juce::Colour(0xff00c8e0u);
    const juce::Colour blue = juce::Colour(0xff0090b8u);
    const juce::Colour darkBg = juce::Colour(0xff040e14u);
    const juce::Colour scopeBg = juce::Colour(0xff040e18u);
    const juce::Colour ledOn = juce::Colour(0xffff7020u);

    inline juce::Colour lfoCol(int i) {
        static juce::Colour c[6] = {
            juce::Colour(0xff00c8e0u),juce::Colour(0xffe85010u),juce::Colour(0xff0090b8u),
            juce::Colour(0xffff8040u),juce::Colour(0xff40b8d0u),juce::Colour(0xffc04010u)
        };
        return c[i % 6];
    }
}

// ── Oscilloscope ──────────────────────────────────────────────────────────
class ScopeDisplay : public juce::Component, public juce::Timer
{
public:
    ScopeDisplay(SlowLFOProcessor& p, int idx) : proc(p), lfoIdx(idx)
    {
        history.assign(HIST, 0.f);
        startTimerHz(30);
    }
    ~ScopeDisplay() override { stopTimer(); }

    void timerCallback() override
    {
        history.erase(history.begin());
        history.push_back(proc.liveValue[lfoIdx].load());
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(KC::scopeBg);
        g.fillRoundedRectangle(b, 2.f);
        g.setColour(KC::border.withAlpha(0.5f));
        g.drawRoundedRectangle(b.reduced(0.5f), 2.f, 1.f);

        g.setColour(KC::border.withAlpha(0.25f));
        g.drawHorizontalLine((int)b.getCentreY(), b.getX()+2, b.getRight()-2);
        for (float x : {0.25f, 0.5f, 0.75f})
            g.drawVerticalLine((int)(b.getX()+b.getWidth()*x), b.getY()+2, b.getBottom()-2);

        g.setColour(KC::lfoCol(lfoIdx).withAlpha(0.9f));
        juce::Path wave;
        float W = b.getWidth()-4, H = b.getHeight()-4;
        float cx = b.getX()+2, cy = b.getCentreY();
        for (int i = 0; i < HIST; ++i) {
            float x = cx + i * W / (HIST - 1);
            float y = cy - history[i] * H * 0.45f;
            if (i == 0) wave.startNewSubPath(x, y);
            else        wave.lineTo(x, y);
        }
        g.strokePath(wave, juce::PathStrokeType(1.5f));
    }

private:
    static constexpr int HIST = 80;
    SlowLFOProcessor& proc;
    int lfoIdx;
    std::vector<float> history;
};

// ── Single LFO strip ─────────────────────────────────────────────────────
class LFOStrip : public juce::Component, public juce::Timer
{
public:
    LFOStrip(SlowLFOProcessor& p, int idx)
        : proc(p), lfoIdx(idx), scope(p, idx)
    {
        auto col = KC::lfoCol(idx);

        // Shape buttons
        const char* shapeNames[] = {"SIN","TRI","SAW","SQR","RND"};
        for (int si = 0; si < 5; ++si) {
            auto* btn = new juce::TextButton(shapeNames[si]);
            btn->setColour(juce::TextButton::buttonColourId,  KC::modBg);
            btn->setColour(juce::TextButton::textColourOffId, KC::textDim);
            int capture = si;
            btn->onClick = [this, capture] { setShape(capture); };
            shapeButtons.add(btn);
            addAndMakeVisible(btn);
        }
        // Mark first shape active
        shapeButtons[0]->setColour(juce::TextButton::buttonColourId,  col.withAlpha(0.25f));
        shapeButtons[0]->setColour(juce::TextButton::textColourOffId, col);

        // Rate knob
        rateKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        rateKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        rateKnob.setColour(juce::Slider::rotarySliderFillColourId,    col);
        rateKnob.setColour(juce::Slider::rotarySliderOutlineColourId, col.withAlpha(0.25f));
        rateKnob.setColour(juce::Slider::thumbColourId,               KC::panelBg);
        addAndMakeVisible(rateKnob);
        rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, p.pid(idx,"rate"), rateKnob);

        // Depth knob
        depthKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        depthKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        depthKnob.setColour(juce::Slider::rotarySliderFillColourId,    KC::orange);
        depthKnob.setColour(juce::Slider::rotarySliderOutlineColourId, KC::orange.withAlpha(0.25f));
        depthKnob.setColour(juce::Slider::thumbColourId,               KC::panelBg);
        addAndMakeVisible(depthKnob);
        depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, p.pid(idx,"depth"), depthKnob);

        // CC spinner
        ccSpinner.setRange(0, 127, 1);
        ccSpinner.setSliderStyle(juce::Slider::IncDecButtons);
        ccSpinner.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 36, 18);
        ccSpinner.setColour(juce::Slider::textBoxTextColourId,       KC::teal);
        ccSpinner.setColour(juce::Slider::textBoxBackgroundColourId, KC::scopeBg);
        ccSpinner.setColour(juce::Slider::textBoxOutlineColourId,    KC::border);
        addAndMakeVisible(ccSpinner);
        ccAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, p.pid(idx,"cc"), ccSpinner);

        // MIDI channel spinner
        chSpinner.setRange(1, 16, 1);
        chSpinner.setSliderStyle(juce::Slider::IncDecButtons);
        chSpinner.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 28, 18);
        chSpinner.setColour(juce::Slider::textBoxTextColourId,       KC::text);
        chSpinner.setColour(juce::Slider::textBoxBackgroundColourId, KC::scopeBg);
        chSpinner.setColour(juce::Slider::textBoxOutlineColourId,    KC::border);
        addAndMakeVisible(chSpinner);
        chAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, p.pid(idx,"ch"), chSpinner);

        // Bi / Uni buttons
        biBtn.setButtonText("BI");
        biBtn.onClick = [this] { setPolar(0); };
        uniBtn.setButtonText("UNI");
        uniBtn.onClick = [this] { setPolar(1); };
        for (auto* b : {&biBtn, &uniBtn}) {
            b->setColour(juce::TextButton::buttonColourId,  KC::modBg);
            b->setColour(juce::TextButton::textColourOffId, KC::textDim);
            addAndMakeVisible(b);
        }
        biBtn.setColour(juce::TextButton::buttonColourId,  KC::orange.withAlpha(0.25f));
        biBtn.setColour(juce::TextButton::textColourOffId, KC::orange);

        // Static labels
        auto initLbl = [&](juce::Label& l, const char* txt, juce::Colour c) {
            l.setText(txt, juce::dontSendNotification);
            l.setFont(juce::Font("Courier New", 7.f, juce::Font::plain));
            l.setColour(juce::Label::textColourId, c);
            l.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(l);
        };
        initLbl(rateLbl,    "RATE",    KC::textDim);
        initLbl(depthLbl,   "DEPTH",   KC::textDim);
        initLbl(ccLbl,      "CC OUT",  KC::textDim);
        initLbl(chLbl,      "MIDI CH", KC::textDim);
        initLbl(hzLbl,      " ",       KC::textDim);
        initLbl(pctLbl,     " ",       KC::orange);
        initLbl(liveCCLbl,  "64",      col);

        addAndMakeVisible(scope);
        startTimerHz(10);
    }

    ~LFOStrip() override { stopTimer(); }

    void timerCallback() override
    {
        float r = (float)rateKnob.getValue();
        float hz = 0.002f * std::pow(1000.f, r);
        hzLbl.setText(hz < 0.1f
            ? juce::String(hz, 3) + " Hz"
            : juce::String(hz, 2) + " Hz",
            juce::dontSendNotification);

        pctLbl.setText(juce::String((int)(depthKnob.getValue()*100)) + "%",
                       juce::dontSendNotification);

        int ccv = proc.liveCCValue[lfoIdx].load();
        liveCCLbl.setText(juce::String(u8"\u2192 ") + juce::String(ccv),
                          juce::dontSendNotification);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced(5);
        a.removeFromTop(16);

        scope.setBounds(a.removeFromTop(44));
        a.removeFromTop(3);

        // Shape buttons
        auto sr = a.removeFromTop(16);
        int sw = sr.getWidth() / 5;
        for (auto* b : shapeButtons)
            b->setBounds(sr.removeFromLeft(sw).reduced(1,0));
        a.removeFromTop(4);

        // Rate knob
        auto rRow = a.removeFromTop(50);
        rateLbl.setBounds(rRow.removeFromTop(12));
        auto rKnobArea = rRow.removeFromLeft(rRow.getWidth()/2);
        rateKnob.setBounds(rKnobArea);
        hzLbl.setBounds(rRow.removeFromTop(12));
        a.removeFromTop(2);

        // Depth knob
        auto dRow = a.removeFromTop(50);
        depthLbl.setBounds(dRow.removeFromTop(12));
        auto dKnobArea = dRow.removeFromLeft(dRow.getWidth()/2);
        depthKnob.setBounds(dKnobArea);
        pctLbl.setBounds(dRow.removeFromTop(12));
        a.removeFromTop(2);

        // CC
        ccLbl.setBounds(a.removeFromTop(12));
        ccSpinner.setBounds(a.removeFromTop(20));
        liveCCLbl.setBounds(a.removeFromTop(14));
        a.removeFromTop(2);

        // CH
        chLbl.setBounds(a.removeFromTop(12));
        chSpinner.setBounds(a.removeFromTop(20));
        a.removeFromTop(4);

        // Bi/Uni
        auto pr = a.removeFromTop(18);
        biBtn .setBounds(pr.removeFromLeft(pr.getWidth()/2).reduced(1,0));
        uniBtn.setBounds(pr.reduced(1,0));
    }

    void paint(juce::Graphics& g) override
    {
        auto col = KC::lfoCol(lfoIdx);
        g.setColour(KC::modBg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);
        g.setColour(col);
        g.fillRect(getLocalBounds().removeFromTop(2).toFloat().reduced(3.f,0.f));
        g.setColour(KC::border.withAlpha(0.6f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.f, 1.f);
        g.setFont(juce::Font("Courier New", 9.f, juce::Font::bold));
        g.setColour(col);
        g.drawText("LFO " + juce::String(lfoIdx+1),
                   getLocalBounds().removeFromTop(15).reduced(5,2),
                   juce::Justification::centred);
    }

private:
    void setShape(int idx)
    {
        proc.apvts.getParameter(proc.pid(lfoIdx,"shape"))
            ->setValueNotifyingHost((float)idx / 4.f);
        auto col = KC::lfoCol(lfoIdx);
        for (int si = 0; si < shapeButtons.size(); ++si) {
            bool on = (si == idx);
            shapeButtons[si]->setColour(juce::TextButton::buttonColourId,
                on ? col.withAlpha(0.25f) : KC::modBg);
            shapeButtons[si]->setColour(juce::TextButton::textColourOffId,
                on ? col : KC::textDim);
        }
    }

    void setPolar(int val)
    {
        proc.apvts.getParameter(proc.pid(lfoIdx,"polar"))
            ->setValueNotifyingHost((float)val);
        bool bi = (val == 0);
        biBtn .setColour(juce::TextButton::buttonColourId,  bi  ? KC::orange.withAlpha(0.25f) : KC::modBg);
        biBtn .setColour(juce::TextButton::textColourOffId, bi  ? KC::orange : KC::textDim);
        uniBtn.setColour(juce::TextButton::buttonColourId, !bi  ? KC::orange.withAlpha(0.25f) : KC::modBg);
        uniBtn.setColour(juce::TextButton::textColourOffId,!bi  ? KC::orange : KC::textDim);
    }

    SlowLFOProcessor& proc;
    int lfoIdx;
    ScopeDisplay scope;

    juce::OwnedArray<juce::TextButton> shapeButtons;
    juce::Slider   rateKnob, depthKnob, ccSpinner, chSpinner;
    juce::TextButton biBtn, uniBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        rateAttach, depthAttach, ccAttach, chAttach;

    // Labels
    juce::Label rateLbl, depthLbl, ccLbl, chLbl, hzLbl, pctLbl, liveCCLbl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOStrip)
};

// ── Main editor ───────────────────────────────────────────────────────────
class SlowLFOEditor : public juce::AudioProcessorEditor
{
public:
    explicit SlowLFOEditor(SlowLFOProcessor& p)
        : AudioProcessorEditor(p), proc(p)
    {
        setSize(780, 420);
        setResizable(true, true);
        setResizeLimits(600, 340, 1200, 640);
        for (int i = 0; i < NUM_LFOS; ++i) {
            strips.add(new LFOStrip(p, i));
            addAndMakeVisible(strips.getLast());
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced(10);
        a.removeFromTop(36);
        int sw = a.getWidth() / NUM_LFOS;
        for (auto* s : strips)
            s->setBounds(a.removeFromLeft(sw).reduced(3,0));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(KC::panelBg);
        g.fillAll();
        g.setColour(KC::border.withAlpha(0.08f));
        for (int y = 0; y < getHeight(); y += 4)
            g.drawHorizontalLine(y, 0.f, (float)getWidth());

        // Screws
        for (auto [x, y] : std::initializer_list<std::pair<float,float>>{
                {8,8},{(float)getWidth()-8,8},{8,(float)getHeight()-8},
                {(float)getWidth()-8,(float)getHeight()-8}}) {
            g.setColour(KC::border.withAlpha(0.4f));
            g.fillEllipse(x-4, y-4, 8, 8);
            g.setColour(KC::panelBg);
            g.drawLine(x-2, y, x+2, y, 1.f);
            g.drawLine(x, y-2, x, y+2, 1.f);
        }

        // Kingfisher logo
        drawKingfisher(g, 12.f, 5.f, 26.f);

        // Title
        g.setFont(juce::Font("Courier New", 15.f, juce::Font::bold));
        g.setColour(KC::text);
        g.drawText("SLOW  LFO",
            getLocalBounds().removeFromTop(36).withX(50).withWidth(200),
            juce::Justification::centredLeft);

        g.setFont(juce::Font("Courier New", 8.f, juce::Font::plain));
        g.setColour(KC::textDim);
        g.drawText("MIDI CC MODULATOR  //  6 INDEPENDENT LFOS",
            getLocalBounds().removeFromTop(36).reduced(12,4),
            juce::Justification::centredRight);
    }

private:
    void drawKingfisher(juce::Graphics& g, float x, float y, float h)
    {
        float s = h / 26.f;
        // Tail
        juce::Path tail;
        tail.addTriangle(x+30*s, y+11*s, x+26*s, y+9*s, x+26*s, y+15*s);
        g.setColour(juce::Colour(0xff007a90u));
        g.fillPath(tail);
        // Body orange
        g.setColour(juce::Colour(0xffc84010u));
        g.fillEllipse(x+8*s, y+9*s, 18*s, 14*s);
        // Back blue
        g.setColour(juce::Colour(0xff0090b0u));
        g.fillEllipse(x+6*s, y+5.5f*s, 20*s, 11*s);
        // Head
        g.setColour(juce::Colour(0xff005870u));
        g.fillEllipse(x+3*s, y+5*s, 12*s, 12*s);
        // Crown
        g.setColour(juce::Colour(0xff003850u));
        g.fillEllipse(x+4*s, y+5.5f*s, 10*s, 5*s);
        // Cheek
        g.setColour(juce::Colour(0xffc8e8f0u).withAlpha(0.8f));
        g.fillEllipse(x+5.5f*s, y+11.5f*s, 5*s, 3*s);
        // Beak
        juce::Path beak;
        beak.addTriangle(x+4*s, y+10.5f*s, x-4*s, y+11.5f*s, x-4*s, y+10.f*s);
        g.setColour(juce::Colour(0xff0a1820u));
        g.fillPath(beak);
        // Eye
        g.setColour(juce::Colour(0xff0a1820u));
        g.fillEllipse(x+5.2f*s, y+8.7f*s, 3.6f*s, 3.6f*s);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillEllipse(x+5.9f*s, y+9.4f*s, 2.2f*s, 2.2f*s);
        g.setColour(juce::Colour(0xff0a1820u));
        g.fillEllipse(x+6.3f*s, y+9.7f*s, 1.f*s, 1.f*s);
        // Wing highlight
        g.setColour(juce::Colour(0xff40c8e0u).withAlpha(0.35f));
        g.fillEllipse(x+12*s, y+7*s, 12*s, 4*s);
    }

    SlowLFOProcessor& proc;
    juce::OwnedArray<LFOStrip> strips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowLFOEditor)
};
