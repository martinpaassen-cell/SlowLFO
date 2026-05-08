#pragma once
#include <JuceHeader.h>
static constexpr int NUM_LFOS = 6;
class SlowLFOProcessor : public juce::AudioProcessor {
public:
    SlowLFOProcessor();
    ~SlowLFOProcessor() override;
    void prepareToPlay(double sr, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Slow LFO"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    std::array<std::atomic<int>,   NUM_LFOS> liveCCValue;
    std::array<std::atomic<float>, NUM_LFOS> liveValue;
    static juce::String pid(int i, const char* p) {
        return "lfo" + juce::String(i) + "_" + p;
    }
private:
    double sampleRate = 44100.0;
    std::array<double, NUM_LFOS> phase{};
    std::array<float,  NUM_LFOS> rndValue{};
    std::array<float,  NUM_LFOS> prevCC{};
    float computeLFO(int i, float hz) noexcept;
    int   toCC(int i, float v, float depth) noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowLFOProcessor)
};
