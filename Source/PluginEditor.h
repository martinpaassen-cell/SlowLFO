#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

static const uint32_t KC_PANEL  = 0xff071820u;
static const uint32_t KC_MOD    = 0xff0b2232u;
static const uint32_t KC_BORDER = 0xff1a4555u;
static const uint32_t KC_TEXT   = 0xffd8eff5u;
static const uint32_t KC_DIM    = 0xff4a7a8au;
static const uint32_t KC_ORANGE = 0xffe85010u;
static const uint32_t KC_TEAL   = 0xff00c8e0u;
static const uint32_t KC_SCOPE  = 0xff040e18u;
static const uint32_t KC_LFO[6] = {
    0xff00c8e0u, 0xffe85010u, 0xff0090b8u,
    0xffff8040u, 0xff40b8d0u, 0xffc04010u
};

class ScopeDisplay : public juce::Component, public juce::Timer
{
public:
    ScopeDisplay(SlowLFOProcessor& p, int idx) : proc(p), lfoIdx(idx)
    { history.assign(HIST, 0.0f); startTimerHz(30); }
    ~ScopeDisplay() override { stopTimer(); }

    void timerCallback() override
    { history.erase(history.begin()); history.push_back(proc.liveValue[lfoIdx].load()); repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(KC_SCOPE)); g.fillRoundedRectangle(b, 2.0f);
        g.setColour(juce::Colour(KC_BORDER).withAlpha(0.3f));
        g.drawHorizontalLine((int)b.getCentreY(), b.getX()+2.0f, b.getRight()-2.0f);
        g.setColour(juce::Colour(KC_LFO[lfoIdx]).withAlpha(0.9f));
        juce::Path wave;
        float W=b.getWidth()-4.0f, H=b.getHeight()-4.0f;
        float cx=b.getX()+2.0f, cy=b.getCentreY();
        for(int i=0;i<HIST;++i){
            float x=cx+(float)i*W/(float)(HIST-1);
            float y=cy-history[(size_t)i]*H*0.45f;
            if(i==0) wave.startNewSubPath(x,y); else wave.lineTo(x,y);
        }
        g.strokePath(wave, juce::PathStrokeType(1.5f));
        g.setColour(juce::Colour(KC_BORDER).withAlpha(0.5f));
        g.drawRoundedRectangle(b.reduced(0.5f),2.0f,1.0f);
    }
private:
    static constexpr int HIST=80;
    SlowLFOProcessor& proc; int lfoIdx;
    std::vector<float> history;
};

class LFOStrip : public juce::Component, public juce::Timer
{
public:
    LFOStrip(SlowLFOProcessor& p, int idx) : proc(p), lfoIdx(idx), scope(p, idx)
    {
        juce::Colour col = juce::Colour(KC_LFO[idx]);
        const char* nm[] = {"SIN","TRI","SAW","SQR","RND"};
        for(int si=0;si<5;++si){
            auto* btn = new juce::TextButton(nm[si]);
            btn->setColour(juce::TextButton::buttonColourId,  juce::Colour(KC_MOD));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colour(KC_DIM));
            int cap=si; btn->onClick=[this,cap]{setShape(cap);};
            shapeButtons.add(btn); addAndMakeVisible(btn);
        }
        shapeButtons[0]->setColour(juce::TextButton::buttonColourId,  col.withAlpha(0.25f));
        shapeButtons[0]->setColour(juce::TextButton::textColourOffId, col);

        auto setupKnob=[&](juce::Slider& sl, juce::Colour c, const char* pid2){
            sl.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            sl.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            sl.setColour(juce::Slider::rotarySliderFillColourId,    c);
            sl.setColour(juce::Slider::rotarySliderOutlineColourId, c.withAlpha(0.25f));
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(KC_PANEL));
            addAndMakeVisible(sl);
            return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                p.apvts, p.pid(idx,pid2), sl);
        };
        rateAttach  = setupKnob(rateKnob,  col,                     "rate");
        depthAttach = setupKnob(depthKnob, juce::Colour(KC_ORANGE), "depth");

        auto setupSpin=[&](juce::Slider& sl, int lo, int hi, const char* pid2){
            sl.setRange(lo,hi,1); sl.setSliderStyle(juce::Slider::IncDecButtons);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft,false,36,18);
            sl.setColour(juce::Slider::textBoxTextColourId,       juce::Colour(KC_TEAL));
            sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(KC_SCOPE));
            sl.setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(KC_BORDER));
            addAndMakeVisible(sl);
            return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                p.apvts, p.pid(idx,pid2), sl);
        };
        ccAttach = setupSpin(ccSpinner, 0, 127, "cc");
        chAttach = setupSpin(chSpinner, 1,  16, "ch");
        chSpinner.setColour(juce::Slider::textBoxTextColourId, juce::Colour(KC_TEXT));

        biBtn.setButtonText("BI");   biBtn.onClick=[this]{setPolar(0);};
        uniBtn.setButtonText("UNI"); uniBtn.onClick=[this]{setPolar(1);};
        for(auto* b:{&biBtn,&uniBtn}){
            b->setColour(juce::TextButton::buttonColourId,  juce::Colour(KC_MOD));
            b->setColour(juce::TextButton::textColourOffId, juce::Colour(KC_DIM));
            addAndMakeVisible(b);
        }
        biBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour(KC_ORANGE).withAlpha(0.25f));
        biBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(KC_ORANGE));

        auto mkL=[&](juce::Label& l, const char* t, uint32_t c2){
            l.setText(t,juce::dontSendNotification);
            l.setFont(juce::Font(9.0f));
            l.setColour(juce::Label::textColourId, juce::Colour(c2));
            l.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(l);
        };
        mkL(rateLbl,  "RATE",    KC_DIM);
        mkL(depthLbl, "DEPTH",   KC_DIM);
        mkL(ccLbl,    "CC OUT",  KC_DIM);
        mkL(chLbl,    "MIDI CH", KC_DIM);
        mkL(hzLbl,    "",        KC_DIM);
        mkL(pctLbl,   "",        KC_ORANGE);
        mkL(liveLbl,  "64",      KC_LFO[idx]);

        addAndMakeVisible(scope);
        startTimerHz(10);
    }
    ~LFOStrip() override { stopTimer(); }

    void timerCallback() override
    {
        float r=(float)rateKnob.getValue();
        float hz=0.002f*std::pow(1000.0f,r);
        hzLbl.setText(hz<0.1f?juce::String(hz,3)+" Hz":juce::String(hz,2)+" Hz",
                      juce::dontSendNotification);
        pctLbl.setText(juce::String((int)(depthKnob.getValue()*100.0))+"%",
                       juce::dontSendNotification);
        liveLbl.setText("-> "+juce::String(proc.liveCCValue[lfoIdx].load()),
                        juce::dontSendNotification);
    }

    void resized() override
    {
        auto a=getLocalBounds().reduced(5); a.removeFromTop(16);
        scope.setBounds(a.removeFromTop(44)); a.removeFromTop(3);
        auto sr=a.removeFromTop(16); int sw=sr.getWidth()/5;
        for(auto* b:shapeButtons) b->setBounds(sr.removeFromLeft(sw).reduced(1,0));
        a.removeFromTop(4);
        rateLbl.setBounds(a.removeFromTop(12));
        auto rr=a.removeFromTop(44);
        rateKnob.setBounds(rr.removeFromLeft(rr.getWidth()/2));
        hzLbl.setBounds(rr.removeFromTop(14));
        depthLbl.setBounds(a.removeFromTop(12));
        auto dr=a.removeFromTop(44);
        depthKnob.setBounds(dr.removeFromLeft(dr.getWidth()/2));
        pctLbl.setBounds(dr.removeFromTop(14));
        a.removeFromTop(2);
        ccLbl.setBounds(a.removeFromTop(12)); ccSpinner.setBounds(a.removeFromTop(20));
        liveLbl.setBounds(a.removeFromTop(14)); a.removeFromTop(2);
        chLbl.setBounds(a.removeFromTop(12)); chSpinner.setBounds(a.removeFromTop(20));
        a.removeFromTop(4);
        auto pr=a.removeFromTop(18);
        biBtn.setBounds(pr.removeFromLeft(pr.getWidth()/2).reduced(1,0));
        uniBtn.setBounds(pr.reduced(1,0));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(KC_MOD));
        g.fillRoundedRectangle(getLocalBounds().toFloat(),3.0f);
        juce::Colour col=juce::Colour(KC_LFO[lfoIdx]);
        g.setColour(col);
        g.fillRect(getLocalBounds().removeFromTop(2).toFloat().reduced(3.0f,0.0f));
        g.setColour(juce::Colour(KC_BORDER).withAlpha(0.6f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),3.0f,1.0f);
        g.setFont(juce::Font(9.0f,juce::Font::bold)); g.setColour(col);
        g.drawText("LFO "+juce::String(lfoIdx+1),
                   getLocalBounds().removeFromTop(15).reduced(5,2),
                   juce::Justification::centred);
    }

private:
    void setShape(int idx2)
    {
        proc.apvts.getParameter(proc.pid(lfoIdx,"shape"))
            ->setValueNotifyingHost((float)idx2/4.0f);
        juce::Colour col=juce::Colour(KC_LFO[lfoIdx]);
        for(int si=0;si<shapeButtons.size();++si){
            bool on=(si==idx2);
            shapeButtons[si]->setColour(juce::TextButton::buttonColourId,
                on?col.withAlpha(0.25f):juce::Colour(KC_MOD));
            shapeButtons[si]->setColour(juce::TextButton::textColourOffId,
                on?col:juce::Colour(KC_DIM));
        }
    }
    void setPolar(int val)
    {
        proc.apvts.getParameter(proc.pid(lfoIdx,"polar"))
            ->setValueNotifyingHost((float)val);
        bool bi=(val==0);
        biBtn.setColour(juce::TextButton::buttonColourId,
            bi?juce::Colour(KC_ORANGE).withAlpha(0.25f):juce::Colour(KC_MOD));
        biBtn.setColour(juce::TextButton::textColourOffId,
            bi?juce::Colour(KC_ORANGE):juce::Colour(KC_DIM));
        uniBtn.setColour(juce::TextButton::buttonColourId,
            !bi?juce::Colour(KC_ORANGE).withAlpha(0.25f):juce::Colour(KC_MOD));
        uniBtn.setColour(juce::TextButton::textColourOffId,
            !bi?juce::Colour(KC_ORANGE):juce::Colour(KC_DIM));
    }

    SlowLFOProcessor& proc; int lfoIdx;
    ScopeDisplay scope;
    juce::OwnedArray<juce::TextButton> shapeButtons;
    juce::Slider rateKnob, depthKnob, ccSpinner, chSpinner;
    juce::TextButton biBtn, uniBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        rateAttach, depthAttach, ccAttach, chAttach;
    juce::Label rateLbl, depthLbl, ccLbl, chLbl, hzLbl, pctLbl, liveLbl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFOStrip)
};

class SlowLFOEditor : public juce::AudioProcessorEditor
{
public:
    explicit SlowLFOEditor(SlowLFOProcessor& p) : AudioProcessorEditor(p), proc(p)
    {
        setSize(780, 420); setResizable(true,true); setResizeLimits(600,340,1200,640);
        for(int i=0;i<NUM_LFOS;++i){ strips.add(new LFOStrip(p,i)); addAndMakeVisible(strips.getLast()); }
    }

    void resized() override
    {
        auto a=getLocalBounds().reduced(10); a.removeFromTop(36);
        int sw=a.getWidth()/NUM_LFOS;
        for(auto* s:strips) s->setBounds(a.removeFromLeft(sw).reduced(3,0));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(KC_PANEL)); g.fillAll();
        g.setColour(juce::Colour(KC_BORDER).withAlpha(0.08f));
        for(int y=0;y<getHeight();y+=4) g.drawHorizontalLine(y,0.0f,(float)getWidth());

        float W=(float)getWidth(), H=(float)getHeight();
        float sx[4]={8.0f,W-8.0f,8.0f,W-8.0f}, sy[4]={8.0f,8.0f,H-8.0f,H-8.0f};
        for(int i=0;i<4;++i){
            g.setColour(juce::Colour(KC_BORDER).withAlpha(0.4f));
            g.fillEllipse(sx[i]-4.0f,sy[i]-4.0f,8.0f,8.0f);
            g.setColour(juce::Colour(KC_PANEL));
            g.drawLine(sx[i]-2.0f,sy[i],sx[i]+2.0f,sy[i],1.0f);
            g.drawLine(sx[i],sy[i]-2.0f,sx[i],sy[i]+2.0f,1.0f);
        }

        drawKingfisher(g,12.0f,5.0f,26.0f);
        g.setFont(juce::Font(15.0f,juce::Font::bold));
        g.setColour(juce::Colour(KC_TEXT));
        g.drawText("SLOW  LFO",juce::Rectangle<int>(50,0,200,36),juce::Justification::centredLeft);
        g.setFont(juce::Font(8.0f)); g.setColour(juce::Colour(KC_DIM));
        g.drawText("MIDI CC MODULATOR  //  6 INDEPENDENT LFOS",
                   getLocalBounds().removeFromTop(36).reduced(12,4),
                   juce::Justification::centredRight);
    }

private:
    void drawKingfisher(juce::Graphics& g,float x,float y,float h)
    {
        float s=h/26.0f;
        juce::Path tail;
        tail.addTriangle(x+30*s,y+11*s,x+26*s,y+9*s,x+26*s,y+15*s);
        g.setColour(juce::Colour(0xff007a90u)); g.fillPath(tail);
        g.setColour(juce::Colour(0xffc84010u)); g.fillEllipse(x+8*s,y+9*s,18*s,14*s);
        g.setColour(juce::Colour(0xff0090b0u)); g.fillEllipse(x+6*s,y+5.5f*s,20*s,11*s);
        g.setColour(juce::Colour(0xff005870u)); g.fillEllipse(x+3*s,y+5*s,12*s,12*s);
        g.setColour(juce::Colour(0xff003850u)); g.fillEllipse(x+4*s,y+5.5f*s,10*s,5*s);
        g.setColour(juce::Colour(0xffc8e8f0u).withAlpha(0.8f)); g.fillEllipse(x+5.5f*s,y+11.5f*s,5*s,3*s);
        juce::Path beak;
        beak.addTriangle(x+4*s,y+10.5f*s,x-4*s,y+11.5f*s,x-4*s,y+10.0f*s);
        g.setColour(juce::Colour(0xff0a1820u)); g.fillPath(beak);
        g.setColour(juce::Colour(0xff0a1820u)); g.fillEllipse(x+5.2f*s,y+8.7f*s,3.6f*s,3.6f*s);
        g.setColour(juce::Colours::white.withAlpha(0.9f)); g.fillEllipse(x+5.9f*s,y+9.4f*s,2.2f*s,2.2f*s);
        g.setColour(juce::Colour(0xff0a1820u)); g.fillEllipse(x+6.3f*s,y+9.7f*s,1.0f*s,1.0f*s);
        g.setColour(juce::Colour(0xff40c8e0u).withAlpha(0.35f)); g.fillEllipse(x+12*s,y+7*s,12*s,4*s);
    }

    SlowLFOProcessor& proc;
    juce::OwnedArray<LFOStrip> strips;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowLFOEditor)
};
