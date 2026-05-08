#include "PluginProcessor.h"
#include "PluginEditor.h"

static const int CC_DEFAULTS[6] = {74, 71, 1, 7, 10, 11};

juce::AudioProcessorValueTreeState::ParameterLayout
SlowLFOProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    for (int i = 0; i < NUM_LFOS; ++i)
    {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            pid(i,"rate"),  "LFO"+juce::String(i+1)+" Rate",
            juce::NormalisableRange<float>(0.f,1.f), 0.1f+i*0.02f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            pid(i,"depth"), "LFO"+juce::String(i+1)+" Depth",
            juce::NormalisableRange<float>(0.f,1.f), 0.8f));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            pid(i,"shape"), "LFO"+juce::String(i+1)+" Shape",
            juce::StringArray{"Sine","Triangle","Saw","Square","Random"}, 0));
        p.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(i,"cc"),    "LFO"+juce::String(i+1)+" CC",
            0, 127, CC_DEFAULTS[i]));
        p.push_back(std::make_unique<juce::AudioParameterInt>(
            pid(i,"ch"),    "LFO"+juce::String(i+1)+" Channel",
            1, 16, 1));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            pid(i,"polar"), "LFO"+juce::String(i+1)+" Polarity",
            juce::StringArray{"Bipolar","Unipolar"}, 0));
    }
    return {p.begin(),p.end()};
}

SlowLFOProcessor::SlowLFOProcessor()
    : AudioProcessor(BusesProperties()),
      apvts(*this,nullptr,"Parameters",createParameters())
{
    for(int i=0;i<NUM_LFOS;++i){
        phase[i]=0.0; rndValue[i]=0.f; prevCC[i]=-1.f;
        liveCCValue[i].store(64); liveValue[i].store(0.f);
    }
}
SlowLFOProcessor::~SlowLFOProcessor(){}

void SlowLFOProcessor::prepareToPlay(double sr,int){ sampleRate=sr; }
void SlowLFOProcessor::releaseResources(){}

float SlowLFOProcessor::computeLFO(int i,float rateHz) noexcept
{
    static const double pi2=2.0*3.14159265358979;
    const double t=std::fmod(phase[i]/pi2,1.0);
    int sh=(int)*apvts.getRawParameterValue(pid(i,"shape"));
    float v=0.f;
    switch(sh){
        case 0: v=(float)std::sin(phase[i]); break;
        case 1: v=(float)(4.0*std::abs(t-0.5)-1.0); break;
        case 2: v=(float)(2.0*t-1.0); break;
        case 3: v=t<0.5?1.f:-1.f; break;
        case 4: v=rndValue[i]; break;
    }
    phase[i]+=rateHz*pi2/sampleRate;
    if(phase[i]>=pi2){
        phase[i]-=pi2;
        if(sh==4) rndValue[i]=juce::Random::getSystemRandom().nextFloat()*2.f-1.f;
    }
    return v;
}

int SlowLFOProcessor::toCC(int i,float v,float depth) noexcept
{
    int polar=(int)*apvts.getRawParameterValue(pid(i,"polar"));
    int cc=polar==0
        ? juce::roundToInt(64.f+v*depth*63.f)
        : juce::roundToInt((v*0.5f+0.5f)*depth*127.f);
    return juce::jlimit(0,127,cc);
}

void SlowLFOProcessor::processBlock(juce::AudioBuffer<float>& buf,juce::MidiBuffer& midi)
{
    juce::ignoreUnused(buf);
    const int n=buf.getNumSamples();
    for(int i=0;i<NUM_LFOS;++i){
        float r01=(float)*apvts.getRawParameterValue(pid(i,"rate"));
        float dep=(float)*apvts.getRawParameterValue(pid(i,"depth"));
        int   cc =(int)*apvts.getRawParameterValue(pid(i,"cc"));
        int   ch =(int)*apvts.getRawParameterValue(pid(i,"ch"));
        float hz =0.002f*std::pow(1000.f,r01);
        float val=0.f;
        for(int s=0;s<n;++s) val=computeLFO(i,hz);
        int ccv=toCC(i,val,dep);
        if(ccv!=(int)prevCC[i]){
            midi.addEvent(juce::MidiMessage::controllerEvent(ch,cc,ccv),0);
            prevCC[i]=(float)ccv;
        }
        liveValue[i].store(val);
        liveCCValue[i].store(ccv);
    }
}

void SlowLFOProcessor::getStateInformation(juce::MemoryBlock& dest){
    auto st=apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(st.createXml());
    copyXmlToBinary(*xml,dest);
}
void SlowLFOProcessor::setStateInformation(const void* d,int sz){
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(d,sz));
    if(xml&&xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new SlowLFOProcessor(); }
