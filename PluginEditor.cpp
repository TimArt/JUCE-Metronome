#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);

    playStopButton.setToggleState(processorRef.isPlaying, juce::NotificationType::dontSendNotification);
    playStopButton.setClickingTogglesState(true);
    playStopButton.onClick = [this]() {
        processorRef.togglePlayback();
        playStopButton.setButtonText(processorRef.isPlaying ? "Stop" : "Play");
    };
    addAndMakeVisible(playStopButton);
    
    bpm.setRange(20.0, 1000.0);
    bpm.setValue(processorRef.bpm);
    bpm.onValueChange = [this]() {
        processorRef.bpm = bpm.getValue();
    };
    addAndMakeVisible(bpm);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    playStopButton.setBounds(bounds.withSizeKeepingCentre(200, 70));
    bpm.setBounds(bounds.removeFromBottom(100));
}
