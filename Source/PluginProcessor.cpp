#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
SlowLFOProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    for (int i = 0; i < NUM_LFOS; ++i)
    {
        // Rate: 0→1 maps to 0.002 Hz → 2 Hz (logarithmic)
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            pid(i,"rate"),  "LFO"+juce::String(i+1)+" Rate",
            juce::NormalisableRange<float>(0.f,1.f), 0.1f + i*0.02f));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            pid(i,"depth"), "LFO"+juce::String(i+1)+" Depth",
            juce::NormalisableRange<float>(0.f,1.f), 0.8f));

        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            pid(i,"shape"), "LFO"+juce::String(i+1)+" Shape",
            juce::StringArray{"Sine","Triangle","Saw","Square","Random"}, 0));

        // CC 0-127
        p.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(i,"cc"),    "LFO"+juce::String(i+1)+" CC",
            0, 127, [74,71,1,7,10,11][i < 6 ? i : 0]));

        // MIDI channel 1-16
        p.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(i,"ch"),    "LFO"+juce::String(i+1)+" Channel",
            1, 16, 1));

        // Polarity: 0=Bipolar, 1=Unipolar
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            pid(i,"polar"), "LFO"+juce::String(i+1)+" Polarity",
            juce::StringArray{"Bipolar","Unipolar"}, 0));
    }
    return {p.begin(), p.end()};
}

SlowLFOProcessor::SlowLFOProcessor()
    : AudioProcessor(BusesProperties()),
      apvts(*this, nullptr, "Parameters", createParameters())
{
    for (int i = 0; i < NUM_LFOS; ++i) {
        phase[i]    = 0.0;
        rndValue[i] = 0.f;
        prevCC[i]   = -1.f;
        liveCCValue[i].store(64);
        liveValue[i].store(0.f);
    }
}

SlowLFOProcessor::~SlowLFOProcessor() {}

void SlowLFOProcessor::prepareToPlay(double sr, int)
{
    sampleRate = sr;
}

void SlowLFOProcessor::releaseResources() {}

// ── Compute one LFO sample ────────────────────────────────────────────────
float SlowLFOProcessor::computeLFO(int i, float rateHz) noexcept
{
    const double p = phase[i];
    const double t = std::fmod(p / (2.0 * 3.14159265), 1.0);
    float v = 0.f;

    int shape = (int)*apvts.getRawParameterValue(pid(i,"shape"));
    switch (shape) {
        case 0: v = (float)std::sin(p); break;                         // sine
        case 1: v = (float)(4.0 * std::abs(t - 0.5) - 1.0); break;    // triangle
        case 2: v = (float)(2.0 * t - 1.0); break;                     // sawtooth
        case 3: v = t < 0.5 ? 1.f : -1.f; break;                       // square
        case 4: v = rndValue[i]; break;                                 // S&H random
        default: v = 0.f;
    }

    // Advance phase
    phase[i] += rateHz * 2.0 * 3.14159265 / sampleRate;
    if (phase[i] >= 2.0 * 3.14159265) {
        phase[i] -= 2.0 * 3.14159265;
        // Update S&H value at each cycle
        if (shape == 4)
            rndValue[i] = juce::Random::getSystemRandom().nextFloat() * 2.f - 1.f;
    }

    return v;
}

// ── Convert LFO value to MIDI CC ─────────────────────────────────────────
int SlowLFOProcessor::toCC(int i, float lfoVal, float depth) noexcept
{
    int polar = (int)*apvts.getRawParameterValue(pid(i,"polar"));
    int cc;
    if (polar == 0) {
        // Bipolar: centre at 64, range ±63*depth
        cc = juce::roundToInt(64.f + lfoVal * depth * 63.f);
    } else {
        // Unipolar: 0 → 127*depth
        cc = juce::roundToInt((lfoVal * 0.5f + 0.5f) * depth * 127.f);
    }
    return juce::jlimit(0, 127, cc);
}

// ── Main process block ────────────────────────────────────────────────────
void SlowLFOProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midi)
{
    juce::ignoreUnused(buffer);

    const int numSamples = buffer.getNumSamples();
    // Send CC updates once per block (at sample 0)
    const int sendAt = 0;

    for (int i = 0; i < NUM_LFOS; ++i)
    {
        float rate01  = (float)*apvts.getRawParameterValue(pid(i,"rate"));
        float depth   = (float)*apvts.getRawParameterValue(pid(i,"depth"));
        int   ccNum   = (int)*apvts.getRawParameterValue(pid(i,"cc"));
        int   channel = (int)*apvts.getRawParameterValue(pid(i,"ch"));

        // Rate: 0.002 Hz → 2 Hz logarithmic
        float rateHz = 0.002f * std::pow(1000.f, rate01);

        // Advance LFO over the block
        float lfoVal = 0.f;
        for (int s = 0; s < numSamples; ++s) {
            lfoVal = computeLFO(i, rateHz);
        }

        // Compute CC value
        int ccVal = toCC(i, lfoVal, depth);

        // Only send if changed (saves MIDI bandwidth)
        if (ccVal != (int)prevCC[i]) {
            auto msg = juce::MidiMessage::controllerEvent(channel, ccNum, ccVal);
            midi.addEvent(msg, sendAt);
            prevCC[i] = (float)ccVal;
        }

        // Update live display values
        liveValue[i].store(lfoVal);
        liveCCValue[i].store(ccVal);
    }
}

void SlowLFOProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void SlowLFOProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlowLFOProcessor();
}
