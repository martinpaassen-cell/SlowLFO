#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ── Kingfisher colour palette ─────────────────────────────────────────────
namespace KC {
    const juce::Colour panelBg   { 0xff071820 };
    const juce::Colour modBg     { 0xff0b2232 };
    const juce::Colour border    { 0xff1a4555 };
    const juce::Colour text      { 0xffd8eff5 };
    const juce::Colour textDim   { 0xff4a7a8a };
    const juce::Colour orange    { 0xffe85010 };  // breast orange
    const juce::Colour teal      { 0xff00c8e0 };  // turquoise
    const juce::Colour blue      { 0xff0090b8 };  // wing blue
    const juce::Colour darkBg    { 0xff040e14 };
    const juce::Colour scopeBg   { 0xff040e18 };
    const juce::Colour ledOn     { 0xffff7020 };

    // Per-LFO accent colours — alternating teal/orange
    inline juce::Colour lfoCol(int i) {
        static juce::Colour cols[6] = {
            {0xff00c8e0},{0xffe85010},{0xff0090b8},
            {0xffff8040},{0xff40b8d0},{0xffc04010}
        };
        return cols[i % 6];
    }
}

// ── Tiny oscilloscope component ───────────────────────────────────────────
class ScopeDisplay : public juce::Component, public juce::Timer
{
public:
    ScopeDisplay(SlowLFOProcessor& p, int idx)
        : proc(p), lfoIdx(idx), history(HIST, 0.f)
    {
        startTimerHz(30);
    }
    ~ScopeDisplay() override { stopTimer(); }

    void timerCallback() override
    {
        float v = proc.liveValue[lfoIdx].load();
        history.erase(history.begin());
        history.push_back(v);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(KC::scopeBg);
        g.fillRoundedRectangle(b, 2.f);
        g.setColour(KC::border.withAlpha(0.5f));
        g.drawRoundedRectangle(b.reduced(0.5f), 2.f, 1.f);

        // Grid
        g.setColour(KC::border.withAlpha(0.25f));
        g.drawHorizontalLine((int)(b.getCentreY()), b.getX()+2, b.getRight()-2);
        for (float x : {0.25f, 0.5f, 0.75f})
            g.drawVerticalLine((int)(b.getX() + b.getWidth()*x), b.getY()+2, b.getBottom()-2);

        // Waveform
        auto col = KC::lfoCol(lfoIdx);
        g.setColour(col.withAlpha(0.9f));
        juce::Path wave;
        float W = b.getWidth()-4, H = b.getHeight()-4;
        float cx = b.getX()+2, cy = b.getCentreY();
        for (int i = 0; i < HIST; ++i) {
            float x = cx + i * W / (HIST - 1);
            float y = cy - history[i] * (H * 0.45f);
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
        for (auto& name : {"SIN","TRI","SAW","SQR","RND"}) {
            auto* btn = new juce::TextButton(name);
            btn->setClickingTogglesState(false);
            btn->setColour(juce::TextButton::buttonColourId,   KC::modBg);
            btn->setColour(juce::TextButton::buttonOnColourId, col.withAlpha(0.25f));
            btn->setColour(juce::TextButton::textColourOffId,  KC::textDim);
            btn->setColour(juce::TextButton::textColourOnId,   col);
            shapeButtons.add(btn);
            addAndMakeVisible(btn);
        }
        int si = 0;
        for (auto* btn : shapeButtons) {
            int captureIdx = si++;
            btn->onClick = [this, captureIdx] { setShape(captureIdx); };
        }

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

        // Channel spinner
        chSpinner.setRange(1, 16, 1);
        chSpinner.setSliderStyle(juce::Slider::IncDecButtons);
        chSpinner.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 28, 18);
        chSpinner.setColour(juce::Slider::textBoxTextColourId,       KC::text);
        chSpinner.setColour(juce::Slider::textBoxBackgroundColourId, KC::scopeBg);
        chSpinner.setColour(juce::Slider::textBoxOutlineColourId,    KC::border);
        addAndMakeVisible(chSpinner);
        chAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, p.pid(idx,"ch"), chSpinner);

        // Polarity buttons (Bi / Uni)
        biBtn.setButtonText("BI");
        biBtn.setClickingTogglesState(false);
        biBtn.onClick = [this]{ setPolar(0); };
        uniBtn.setButtonText("UNI");
        uniBtn.setClickingTogglesState(false);
        uniBtn.onClick = [this]{ setPolar(1); };
        for (auto* b : {&biBtn, &uniBtn}) {
            b->setColour(juce::TextButton::buttonColourId,  KC::modBg);
            b->setColour(juce::TextButton::textColourOffId, KC::textDim);
            addAndMakeVisible(b);
        }
        updatePolarButtons();

        addAndMakeVisible(scope);
        startTimerHz(10);
    }

    ~LFOStrip() override { stopTimer(); }

    void timerCallback() override
    {
        // Update Hz label
        float rate01 = (float)rateKnob.getValue();
        float hz = 0.002f * std::pow(1000.f, rate01);
        hzLabel.setText(hz < 0.1f
            ? juce::String(hz, 3) + " Hz"
            : juce::String(hz, 2) + " Hz",
            juce::dontSendNotification);

        // Update depth % label
        pctLabel.setText(juce::String((int)(depthKnob.getValue() * 100)) + "%",
                         juce::dontSendNotification);

        // Update live CC label
        int cc = proc.liveCCValue[lfoIdx].load();
        liveCCLabel.setText("→ " + juce::String(cc), juce::dontSendNotification);
        liveCCLabel.setColour(juce::Label::textColourId, KC::lfoCol(lfoIdx));
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced(5);
        a.removeFromTop(16); // lfo number label drawn in paint

        scope.setBounds(a.removeFromTop(44));
        a.removeFromTop(3);

        // Shape buttons
        auto shpRow = a.removeFromTop(16);
        int sw = shpRow.getWidth() / 5;
        for (auto* b : shapeButtons)
            b->setBounds(shpRow.removeFromLeft(sw).reduced(1,0));
        a.removeFromTop(4);

        // Rate knob + hz label
        auto rRow = a.removeFromTop(50);
        auto rLeft = rRow.removeFromLeft(rRow.getWidth()/2);
        rateLbl.setBounds(rLeft.removeFromTop(12));
        rateKnob.setBounds(rLeft);
        hzLabel.setBounds(rRow.removeFromTop(12).translated(0,-1));
        rRow.removeFromTop(4);

        // Depth knob + pct label
        auto dRow = a.removeFromTop(50);
        auto dLeft = dRow.removeFromLeft(dRow.getWidth()/2);
        depthLbl.setBounds(dLeft.removeFromTop(12));
        depthKnob.setBounds(dLeft);
        pctLabel.setBounds(dRow.removeFromTop(12).translated(0,-1));
        dRow.removeFromTop(4);

        a.removeFromTop(2);

        // CC spinner + live label
        ccLbl.setBounds(a.removeFromTop(12));
        ccSpinner.setBounds(a.removeFromTop(20));
        liveCCLabel.setBounds(a.removeFromTop(14));
        a.removeFromTop(2);

        // CH spinner
        chLbl.setBounds(a.removeFromTop(12));
        chSpinner.setBounds(a.removeFromTop(20));
        a.removeFromTop(4);

        // Bi/Uni buttons
        auto polRow = a.removeFromTop(18);
        biBtn.setBounds(polRow.removeFromLeft(polRow.getWidth()/2).reduced(1,0));
        uniBtn.setBounds(polRow.reduced(1,0));
    }

    void paint(juce::Graphics& g) override
    {
        auto col = KC::lfoCol(lfoIdx);

        // Module background
        g.setColour(KC::modBg);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        // Top accent line
        g.setColour(col);
        g.fillRect(getLocalBounds().removeFromTop(2).toFloat().withTrimmedLeft(3).withTrimmedRight(3));

        // Border
        g.setColour(KC::border.withAlpha(0.6f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.f, 1.f);

        // LFO number
        g.setFont(juce::Font("Courier New", 9.f, juce::Font::bold));
        g.setColour(col);
        g.drawText("LFO " + juce::String(lfoIdx+1),
                   getLocalBounds().removeFromTop(15).reduced(5,2),
                   juce::Justification::centred);
    }

private:
    void setShape(int idx) {
        proc.apvts.getParameter(proc.pid(lfoIdx,"shape"))
            ->setValueNotifyingHost((float)idx / 4.f);
        for (int si = 0; si < shapeButtons.size(); ++si) {
            auto col = KC::lfoCol(lfoIdx);
            shapeButtons[si]->setColour(juce::TextButton::buttonColourId,
                si == idx ? col.withAlpha(0.25f) : KC::modBg);
            shapeButtons[si]->setColour(juce::TextButton::textColourOffId,
                si == idx ? col : KC::textDim);
        }
    }

    void setPolar(int val) {
        proc.apvts.getParameter(proc.pid(lfoIdx,"polar"))
            ->setValueNotifyingHost((float)val);
        updatePolarButtons();
    }

    void updatePolarButtons() {
        int cur = (int)(*proc.apvts.getRawParameterValue(proc.pid(lfoIdx,"polar")));
        biBtn .setColour(juce::TextButton::buttonColourId,
            cur==0 ? KC::orange.withAlpha(0.25f) : KC::modBg);
        biBtn .setColour(juce::TextButton::textColourOffId, cur==0 ? KC::orange : KC::textDim);
        uniBtn.setColour(juce::TextButton::buttonColourId,
            cur==1 ? KC::orange.withAlpha(0.25f) : KC::modBg);
        uniBtn.setColour(juce::TextButton::textColourOffId, cur==1 ? KC::orange : KC::textDim);
    }

    auto makeLbl(const char* txt) {
        juce::Label* l = new juce::Label();
        l->setText(txt, juce::dontSendNotification);
        l->setFont(juce::Font("Courier New", 7.f, juce::Font::plain));
        l->setColour(juce::Label::textColourId, KC::textDim);
        l->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(l);
        return l;
    }

    SlowLFOProcessor& proc;
    int lfoIdx;
    ScopeDisplay scope;

    juce::OwnedArray<juce::TextButton> shapeButtons;
    juce::Slider rateKnob, depthKnob, ccSpinner, chSpinner;
    juce::TextButton biBtn, uniBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        rateAttach, depthAttach, ccAttach, chAttach;

    // Labels — stored as owned pointers
    struct Labels {
        juce::Label rate{"","RATE"}, depth{"","DEPTH"}, cc{"","CC OUT"}, ch{"","MIDI CH"};
        juce::Label hz{""," "}, pct{""," "}, liveCC{"","64"};
    } labels;

    juce::Label& rateLbl    = labels.rate;
    juce::Label& depthLbl   = labels.depth;
    juce::Label& ccLbl      = labels.cc;
    juce::Label& chLbl      = labels.ch;
    juce::Label& hzLabel    = labels.hz;
    juce::Label& pctLabel   = labels.pct;
    juce::Label& liveCCLabel= labels.liveCC;

    void initLabels() {
        auto styleLabel = [&](juce::Label& l, const char* txt, juce::Colour col=KC::textDim){
            l.setText(txt, juce::dontSendNotification);
            l.setFont(juce::Font("Courier New", 7.f, juce::Font::plain));
            l.setColour(juce::Label::textColourId, col);
            l.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(l);
        };
        styleLabel(rateLbl,   "RATE");
        styleLabel(depthLbl,  "DEPTH");
        styleLabel(ccLbl,     "CC OUT");
        styleLabel(chLbl,     "MIDI CH");
        styleLabel(hzLabel,   " ", KC::textDim);
        styleLabel(pctLabel,  " ", KC::orange);
        styleLabel(liveCCLabel,"64", KC::teal);
    }

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
            auto* s = new LFOStrip(p, i);
            strips.add(s);
            addAndMakeVisible(s);
        }
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced(10);
        // Header
        a.removeFromTop(32);
        // 6 strips
        int sw = a.getWidth() / NUM_LFOS;
        for (auto* s : strips)
            s->setBounds(a.removeFromLeft(sw).reduced(3, 0));
    }

    void paint(juce::Graphics& g) override
    {
        // Panel background
        g.setColour(KC::panelBg);
        g.fillAll();

        // Subtle texture lines
        g.setColour(KC::border.withAlpha(0.08f));
        for (int y = 0; y < getHeight(); y += 4)
            g.drawHorizontalLine(y, 0.f, (float)getWidth());

        // Corner screws
        auto drawScrew = [&](float x, float y) {
            g.setColour(KC::border.withAlpha(0.4f));
            g.fillEllipse(x-4, y-4, 8, 8);
            g.setColour(KC::panelBg);
            g.drawLine(x-2, y, x+2, y, 1.f);
            g.drawLine(x, y-2, x, y+2, 1.f);
        };
        drawScrew(8, 8);
        drawScrew(getWidth()-8, 8);
        drawScrew(8, getHeight()-8);
        drawScrew(getWidth()-8, getHeight()-8);

        // Header
        auto hdr = getLocalBounds().removeFromTop(36).reduced(12, 4);

        // Kingfisher SVG logo (drawn procedurally)
        drawKingfisher(g, 12.f, 4.f, 28.f);

        // Title
        g.setFont(juce::Font("Courier New", 15.f, juce::Font::bold));
        g.setColour(KC::text);
        g.drawText("SLOW  LFO", hdr.withX(50), juce::Justification::centredLeft);

        g.setFont(juce::Font("Courier New", 8.f, juce::Font::plain));
        g.setColour(KC::textDim);
        g.drawText("MIDI CC MODULATOR  //  6 INDEPENDENT LFOS",
                   hdr, juce::Justification::centredRight);
    }

private:
    void drawKingfisher(juce::Graphics& g, float x, float y, float h)
    {
        float s = h / 26.f;
        auto t = [&](float px, float py) -> juce::Point<float> {
            return {x + px*s, y + py*s};
        };

        // Tail
        juce::Path tail;
        tail.addTriangle(t(30,11), t(26,9), t(26,15));
        g.setColour(juce::Colour(0xff007a90));
        g.fillPath(tail);

        // Body - orange
        g.setColour(juce::Colour(0xffc84010));
        g.fillEllipse(x+17*s-9*s, y+16*s-7*s, 18*s, 14*s);

        // Back - blue
        g.setColour(juce::Colour(0xff0090b0));
        g.fillEllipse(x+16*s-10*s, y+11*s-5.5f*s, 20*s, 11*s);

        // Head
        g.setColour(juce::Colour(0xff005870));
        g.fillEllipse(x+9*s-6*s, y+11*s-6*s, 12*s, 12*s);

        // Crown
        g.setColour(juce::Colour(0xff003850));
        g.fillEllipse(x+9*s-5*s, y+8*s-2.5f*s, 10*s, 5*s);

        // Cheek
        g.setColour(juce::Colour(0xffc8e8f0).withAlpha(0.8f));
        g.fillEllipse(x+8*s-2.5f*s, y+13*s-1.5f*s, 5*s, 3*s);

        // Beak
        juce::Path beak;
        beak.addTriangle(t(4,10.5f), t(-4,11.5f), t(-4,10.f));
        g.setColour(juce::Colour(0xff0a1820));
        g.fillPath(beak);

        // Eye
        g.setColour(juce::Colour(0xff0a1820));
        g.fillEllipse(x+7*s-1.8f*s, y+10.5f*s-1.8f*s, 3.6f*s, 3.6f*s);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillEllipse(x+7*s-1.1f*s, y+10.5f*s-1.1f*s, 2.2f*s, 2.2f*s);
        g.setColour(juce::Colour(0xff0a1820));
        g.fillEllipse(x+7.3f*s-0.5f*s, y+10.2f*s-0.5f*s, 1.f*s, 1.f*s);

        // Highlight
        g.setColour(juce::Colour(0xff40c8e0).withAlpha(0.35f));
        g.fillEllipse(x+18*s-6*s, y+9*s-2*s, 12*s, 4*s);
    }

    SlowLFOProcessor& proc;
    juce::OwnedArray<LFOStrip> strips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowLFOEditor)
};
