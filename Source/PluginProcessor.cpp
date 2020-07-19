/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewTemplateAudioProcessor::NewTemplateAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),apvts(*this, nullptr, "Parameters", createParameters())
#endif
{
    apvts.state.addListener(this); //add a listener in this processor to this value tree state
    
    init();
}

NewTemplateAudioProcessor::~NewTemplateAudioProcessor()
{
}

//==============================================================================
const juce::String NewTemplateAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewTemplateAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NewTemplateAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NewTemplateAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NewTemplateAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewTemplateAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NewTemplateAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewTemplateAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NewTemplateAudioProcessor::getProgramName (int index)
{
    return {};
}

void NewTemplateAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NewTemplateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepare(sampleRate,samplesPerBlock);
    update();
    reset();
    isActive=true;
}

void NewTemplateAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewTemplateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void NewTemplateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if(!isActive)
    {
        return;
    }
    
    if(mustUpdateProcessing)
    {
        update();
    }
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();
    auto numChannels = jmin(totalNumInputChannels,totalNumOutputChannels);
    
    auto sumMaxVal = 0.0f;
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);
    
    
    //outputVolume.applyGain(buffer, buffer.getNumSamples());
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        auto channelMaxVal = 0.0f;
        auto currentMaxVal = meterGlobalMaxVal.load();
        
        //Filter Audio
        lowPass[channel].processSamples(channelData, numSamples);
        highPass[channel].processSamples(channelData, numSamples);
        lowMid[channel].processSamples(channelData, numSamples);
        midFilter[channel].processSamples(channelData, numSamples);
        highMid[channel].processSamples(channelData, numSamples);
        
        
        //Adjust Volume
        outputVolume[channel].applyGain(channelData,numSamples);
        //get absolute value of all samples in a buffer
        //find largest value in that buffer
        for(int sample =0;sample < numSamples;++sample)
        {
            auto rectifiedVal = std::abs(channelData[sample]);
            
            if(channelMaxVal < rectifiedVal)
                channelMaxVal = rectifiedVal;
            
            if(currentMaxVal < rectifiedVal)
                currentMaxVal = rectifiedVal;
        }
        
        sumMaxVal +=channelMaxVal;
        meterGlobalMaxVal.store(currentMaxVal);
        
        for(int sample=0;sample<numSamples;++sample)
        {
            //channelData[sample] *= outputVolume;
            //Iterate through the samples, hard clip values
            channelData[sample] = jlimit(-1.0f, 1.0f, channelData[sample]);
        }
    }
    meterLocalVal.store(sumMaxVal / (float) numChannels);
}

//==============================================================================
bool NewTemplateAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NewTemplateAudioProcessor::createEditor()
{
    //return new NewTemplateAudioProcessorEditor (*this);
    return new GenericAudioProcessorEditor (*this);
}

//==============================================================================
void NewTemplateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    ValueTree copyState = apvts.copyState();
    std::unique_ptr<XmlElement> xml=copyState.createXml();
    copyXmlToBinary(*xml.get(), destData);
}

void NewTemplateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml= getXmlFromBinary(data,sizeInBytes);
    ValueTree copyState = ValueTree::fromXml(*xml.get());
    apvts.replaceState(copyState);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewTemplateAudioProcessor();
}

void NewTemplateAudioProcessor::init()
{
    //called once to initialize values to dsp
    
}

void NewTemplateAudioProcessor::prepare(double sampleRate, int samplesPerBlock)
{
    // Pass sample rate and buffer size to DSP
    
}

void NewTemplateAudioProcessor::update()
{
    //Update DSP when user changes parameters
    mustUpdateProcessing=false;
    
    auto lpfCutoff = apvts.getRawParameterValue("LPF");
    auto lpfQ = apvts.getRawParameterValue("LPFQ");
    
    auto hpfCutoff = apvts.getRawParameterValue("HPF");
    auto hpfQ = apvts.getRawParameterValue("HPFQ");
    
    auto lowMidCentre = apvts.getRawParameterValue("LOMIDF");
    auto lowMidQ = apvts.getRawParameterValue("LOMIDQ");
    auto lowMidGain = apvts.getRawParameterValue("LOMIDGAIN");
    
    auto midCentre = apvts.getRawParameterValue("MIDF");
    auto midQ = apvts.getRawParameterValue("MIDQ");
    auto midGain = apvts.getRawParameterValue("MIDGAIN");
    
    auto highMidCentre = apvts.getRawParameterValue("HIMIDF");
    auto highMidQ = apvts.getRawParameterValue("HIMIDQ");
    auto highMidGain = apvts.getRawParameterValue("HIMIDGAIN");
    
    //==============================================================================
    auto outVolume = apvts.getRawParameterValue("OUT");
    auto sr=getSampleRate();
    
    for(int channel=0;channel<2;++channel)
    {
        lowPass[channel].setCoefficients(IIRCoefficients::makeLowPass (sr, lpfCutoff->load(),lpfQ->load()));
        highPass[channel].setCoefficients(IIRCoefficients::makeHighPass(sr, hpfCutoff->load(), hpfQ->load()));
        lowMid[channel].setCoefficients(IIRCoefficients::makePeakFilter(sr, lowMidCentre->load(), lowMidQ->load(), Decibels::decibelsToGain(lowMidGain->load())));
        midFilter[channel].setCoefficients(IIRCoefficients::makePeakFilter(sr, midCentre->load(), midQ->load(), Decibels::decibelsToGain(midGain->load())));
        highMid[channel].setCoefficients(IIRCoefficients::makePeakFilter(sr, highMidCentre->load(), highMidQ->load(), Decibels::decibelsToGain(highMidGain->load())));
        
        //==============================================================================
        outputVolume[channel].setTargetValue(Decibels::decibelsToGain(outVolume->load()));
    }
    
    
}

void NewTemplateAudioProcessor::reset()
{
    // Reset DSP parameters
    for(int channel=0;channel<2;++channel)
    {
        lowPass[channel].reset();
        highPass[channel].reset();
        lowMid[channel].reset();
        midFilter[channel].reset();
        highMid[channel].reset();
        
        outputVolume[channel].reset(getSampleRate(), 0.010);
    }
    meterLocalVal.store(0.0f);
    meterGlobalMaxVal.store(0.0f);
}

AudioProcessorValueTreeState::ParameterLayout NewTemplateAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> parameters;
    //create parameters
    //add to vector
    
    std::function<String(float,int)> valueToTextFunction=[](float x, int l) {return String(x,4);};
    std::function<float(const String&)> textToValueFunction=[](const String& str) {return str.getFloatValue();};
    //==============================================================================
    //FILTERS
    
    //LowPass
    parameters.push_back(std::make_unique<AudioParameterFloat>("LPF","Low Pass Cutoff Frequency",NormalisableRange<float>(20.0f,22000.0f,1.0f,0.95f),20000.0f,"Hz",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("LPFQ","Low Pass Q",NormalisableRange<float>(0.0f,20.0f,1.0f,0.25f),1.0f,"Q",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    
    //High Pass
    parameters.push_back(std::make_unique<AudioParameterFloat>("HPF","High Pass Cutoff Frequency",NormalisableRange<float>(20.0f,22000.0f,1.0f,0.2f),20.0f,"Hz",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("HPFQ","High Pass Q",NormalisableRange<float>(0.0f,20.0f,1.0f,0.25f),1.0f,"Q",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    
    //Low Mid
    parameters.push_back(std::make_unique<AudioParameterFloat>("LOMIDF","Low-Mid Centre Frequency",NormalisableRange<float>(20.0f,300.0f,1.0f,0.8f),200.0f,"Hz",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("LOMIDQ","Low-Mid Q",NormalisableRange<float>(0.0f,20.0f,1.0f,0.25f),1.0f,"Q",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("LOMIDGAIN","Low-Mid Gain",NormalisableRange<float>(-40.0f,40.0f),0.0f,"dB",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    
    //Mid
    parameters.push_back(std::make_unique<AudioParameterFloat>("MIDF","Mid Centre Frequency",NormalisableRange<float>(250.0f,2500.0f,1.0f,0.5f),800.0f,"Hz",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("MIDQ","Mid Q",NormalisableRange<float>(0.0f,20.0f,1.0f,0.25f),1.0f,"Q",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("MIDGAIN","Mid Gain",NormalisableRange<float>(-40.0f,40.0f),0.0f,"dB",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    
    //High Mid
    parameters.push_back(std::make_unique<AudioParameterFloat>("HIMIDF","High-Mid Centre Frequency",NormalisableRange<float>(2000.0f,20000.0f,1.0f,0.5f),4000.0f,"Hz",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("HIMIDQ","High-Mid Q",NormalisableRange<float>(0.0f,20.0f,1.0f,0.25f),1.0f,"Q",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    parameters.push_back(std::make_unique<AudioParameterFloat>("HIMIDGAIN","High-Mid Gain",NormalisableRange<float>(-40.0f,40.0f),0.0f,"dB",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    
    //==============================================================================
    //Volume
    parameters.push_back(std::make_unique<AudioParameterFloat>("OUT","Volume",NormalisableRange<float>(-40.0f,40.0f),0.0f,"dB",AudioProcessorParameter::genericParameter,valueToTextFunction,textToValueFunction));
    
    return { parameters.begin(), parameters.end() };
}
