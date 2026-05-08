#pragma once
#include <JuceHeader.h>

static constexpr int NUM_LFOS = 6;

// LFO shapes
enum class LFOShape { Sine=0, Triangle, Saw, Square, Random };

class SlowLFOProcessor : public juce::AudioProcessor
{
public:
    SlowLFOProcessor();
    ~SlowLFOProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Slow LFO"; }
    bool  acceptsMidi()  const override { return true; }
    bool  producesMidi() const override { return true; }
    bool  isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // Live CC output values (0-127) for display — read by editor
    std::array<std::atomic<int>, NUM_LFOS> liveCCValue;
    // Live LFO raw values (-1..1) for oscilloscope
    std::array<std::atomic<float>, NUM_LFOS> liveValue;

    static juce::String pid(int i, const char* p) {
        return "lfo" + juce::String(i) + "_" + p;
    }

private:
    double sampleRate = 44100.0;

    // LFO state
    std::array<double, NUM_LFOS> phase {};
    std::array<float,  NUM_LFOS> rndValue {};
    std::array<float,  NUM_LFOS> prevCC {};

    float computeLFO(int i, float rateHz) noexcept;
    int   toCC(int i, float lfoVal, float depth) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowLFOProcessor)
};
