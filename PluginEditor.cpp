#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);

    playStopButton.setToggleState (processorRef.isPlaying, juce::NotificationType::dontSendNotification);
    playStopButton.setClickingTogglesState (true);
    playStopButton.onClick = [this]()
    {
        processorRef.togglePlayback();
        playStopButton.setButtonText (processorRef.isPlaying ? "Stop" : "Play");
    };
    addAndMakeVisible (playStopButton);

    bpm.setRange (20.0, 1000.0);
    bpm.setValue (processorRef.bpm);
    bpm.onValueChange = [this]()
    {
        processorRef.bpm = bpm.getValue();
    };
    addAndMakeVisible (bpm);

    auto setupLabel = [this] (juce::Label& label, const bool isNumerator)
    {
        label.setEditable (true);

        if (isNumerator)
        {
            label.onTextChange = [&label, this]()
            {
                const auto text = label.getText();
                int value = text.getIntValue();
                value = std::clamp (value, 1, 99);

                label.setText (juce::String (value), juce::dontSendNotification);
                processorRef.timeSigNumerator = value;
            };
        }
        else
        {
            label.onTextChange = [&label, this]()
            {
                const auto text = label.getText();
                const int inputValue = text.getIntValue();
                constexpr std::array validDenominators = { 1, 2, 4, 8, 16, 32, 64 };

                int closestValidValue = validDenominators[0];
                for (const auto& denominator : validDenominators)
                {
                    const int distance = std::abs (denominator - inputValue);
                    if (distance < std::abs (closestValidValue - inputValue))
                    {
                        closestValidValue = denominator;
                    }
                }

                label.setText (juce::String (closestValidValue), juce::dontSendNotification);
                processorRef.timeSigDenominator = closestValidValue;
            };
        }

        addAndMakeVisible (label);
    };

    setupLabel (timeSignatureNumerator, true);
    setupLabel (timeSignatureDenominator, false);
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
    constexpr int padding = 8;

    auto bounds = getLocalBounds();
    playStopButton.setBounds (bounds.removeFromTop (bounds.getHeight() * 2 / 3).reduced (padding));

    const auto bottomHeight = bounds.getHeight();
    bpm.setBounds (bounds.removeFromTop (bottomHeight / 2).reduced (padding));

    auto left = bounds.removeFromLeft (bounds.getWidth() / 2);
    auto right = bounds;
    constexpr int timeSignatureLabelWidth = 50;
    timeSignatureNumerator.setBounds (left.removeFromRight (timeSignatureLabelWidth).reduced (padding));
    timeSignatureDenominator.setBounds (right.removeFromLeft (timeSignatureLabelWidth).reduced (padding));
}
