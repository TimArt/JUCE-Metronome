
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <span>

class Metronome
{
public:
    static constexpr int MAX_BEATS_IN_AUDIO_BUFFER_PREDICTION = 2048;

    Metronome()
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        auto* reader = formatManager.createReaderFor (juce::File ("/Users/timarterbury/Documents/Dev Projects/JUCE Metronome/Resources/click-beat.wav"));
        if (reader == nullptr)
        {
            jassertfalse;
            return;
        }
        sample = std::make_unique<juce::AudioFormatReaderSource> (reader, true);

        beatPositions.reserve (MAX_BEATS_IN_AUDIO_BUFFER_PREDICTION); // Be able to handle enough beat positions per buffer without reallocation
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        this->sampleRate = spec.sampleRate;
        sample->prepareToPlay (static_cast<int> (spec.maximumBlockSize), spec.sampleRate);
        reset();
    }

    void setBPM(double bpmNew)
    {
        if (!juce::exactlyEqual(bpm, bpmNew))
        {
            bpm = bpmNew;
            countOfCurrentBeat = std::min(countOfCurrentBeat, getSamplesPerBeat()); // TODO: Ensure this is correct instead of getSamplesPerBeat() - 1
        }
    }

    void reset()
    {
        isFirstBeat = true;
        countOfCurrentBeat = 0;
        countBeatInMeasure = 0;

        beatPositions.clear();

        sample->setNextReadPosition (0);
    }

    // template <typename ProcessContext>
    // void process (const ProcessContext& context) noexcept
    // {
    //     const auto& inputBlock = context.getInputBlock();
    //     auto& outputBlock = context.getOutputBlock();
    //     const size_t numChannels = outputBlock.getNumChannels();
    //     const size_t numSamples = outputBlock.getNumSamples();
    //     jassert (inputBlock.getNumChannels() == numChannels);
    //     jassert (inputBlock.getNumSamples() == numSamples);
    //
    //     if (! enabled || context.isBypassed)
    //     {
    //         outputBlock.copyFrom (inputBlock);
    //         return;
    //     }
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        // const auto& inputBlock = context.getInputBlock();
        // auto& outputBlock = context.getOutputBlock();
        // const size_t numChannels = outputBlock.getNumChannels();
        // const size_t numSamples = outputBlock.getNumSamples();
        // jassert (inputBlock.getNumChannels() == numChannels);
        // jassert (inputBlock.getNumSamples() == numSamples);

        // const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        // TODO: Add in measure counting
        // When metronome first starts playing, trigger audio to start rendering
        if (isFirstBeat)
        {
            isFirstBeat = false;
            beatPositions.push_back (0);
        }

        // Determine remaining beat positions for this block
        const int samplesPerBeat = getSamplesPerBeat();
        int offset = samplesPerBeat - countOfCurrentBeat;
        while (offset < numSamples)
        {
            beatPositions.push_back(offset);
            offset += samplesPerBeat;
        }

        // If no beat positions in this block, keep counting and output any audio that may be remaining in the beat sample
        if (beatPositions.empty())
        {
            countOfCurrentBeat += numSamples;

            const juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
            //info.clearActiveBufferRegion(); // clear before filling via sample->getNextAudioBlock(info), do I really need to do this?
            sample->getNextAudioBlock (info);
        }
        else // If beat positions in this block, restart audio sample and start outputting it at the expected position
        {
            countOfCurrentBeat = numSamples - beatPositions.back();

            // Output sample beginning at corresponding beat positions
            for (const auto & beatOffset : beatPositions)
            {
                sample->setNextReadPosition (0);

                const juce::AudioSourceChannelInfo info (&buffer, beatOffset, numSamples - beatOffset); // allocation?? looks like lightweight struct
                //info.clearActiveBufferRegion(); // clear before filling via sample->getNextAudioBlock(info), do I really need to do this?
                sample->getNextAudioBlock (info);
            }
        }

        beatPositions.clear();

        /*
            for (size_t n = 0; n < numSamples; ++n)
        {
            while (countSamples > samplesPerBeat)
            {
                countSamples -= samplesPerBeat;
                countBeatInMeasure++;
            }

            countSamples++;

            for (size_t ch = 0; ch < numChannels; ++ch)
                outputBlock.getChannelPointer (ch)[n] = processSample (inputBlock.getChannelPointer (ch)[n], ch);
        }
        */
    }

    struct TimeSignature
    {
        int numerator = 4; // beats per measure (downbeat tracking)
        int denominator = 4; // note which receives beat (4 = quarter, 8 = eighth, 16 = sixteenth)
    };

    TimeSignature timeSignature = { 4, 4 };

private:

    double bpm = 120;

    [[nodiscard]] int getSamplesPerBeat() const { return juce::roundToInt (60.0 / bpm * sampleRate); }

    bool isFirstBeat = true;

    double sampleRate = 0;
    int countOfCurrentBeat = 0;
    int countBeatInMeasure = 0;

    //const int triangleWaveLengthSeconds = 0.01;
    //int triangleWaveLengthSamples = 0;

    // Ideally I would have a downbeat sample and a regular beat sample
    // Also, ideally if mutliple metronone samples overlapped I would crossfade between them to prevent popping, or
    // I would combine them over the top of each other, having a certain number of voices in case they overlap. I would
    // need timeSignature.denominator number of voices if they overlapped.
    // For now, lets just use one . . .
    std::unique_ptr<juce::AudioFormatReaderSource> sample;

    std::vector<int> beatPositions;

    //juce::AudioPluginFormatManager formatManager;
    //juce::AudioFormatReaderSource
};
