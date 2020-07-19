/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class NewTemplateAudioProcessor  : public juce::AudioProcessor,
public juce::ValueTree::Listener
{
public:
    //==============================================================================
    NewTemplateAudioProcessor();
    ~NewTemplateAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    void init(); //called once to initialize values to dsp
    void prepare(double sampleRate, int samplesPerBlock); // Pass sample rate and buffer size to DSP
    void update(); //Update DSP when user changes parameters
    void reset() override; // Reset DSP parameters
    
    AudioProcessorValueTreeState apvts;
    AudioProcessorValueTreeState::ParameterLayout createParameters();
    
    std::atomic<float> meterLocalVal,meterGlobalMaxVal;
private:
    bool isActive={false};
    bool mustUpdateProcessing {false};
    //float outputVolume {0.0f};
    
    //Filters
    IIRFilter lowPass [2];
    IIRFilter lowMid[2];
    IIRFilter midFilter[2];
    IIRFilter highMid[2];
    IIRFilter highPass[2];
    //outputVolume
    LinearSmoothedValue<float> outputVolume[2]{0.0};
    
    
    
    void valueTreePropertyChanged (ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) override
    {
        //Detect when a user changes parameters
        mustUpdateProcessing=true;
    }
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewTemplateAudioProcessor)
};
