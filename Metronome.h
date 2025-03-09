
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <span>

class Metronome
{
public:
    Metronome()
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto makeReaderSource = [&formatManager] (const juce::String& filename)
        {
            auto* reader = formatManager.createReaderFor (juce::File (filename));
            if (reader == nullptr)
            {
                jassertfalse;
            }
            return std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        };

        downbeatAudio = makeReaderSource ("/Users/timarterbury/Documents/Dev Projects/JUCE Metronome/Resources/click-downbeat.wav");
        beatAudio = makeReaderSource ("/Users/timarterbury/Documents/Dev Projects/JUCE Metronome/Resources/click-beat.wav");

        // Be able to handle enough beat positions per buffer without reallocation
        static constexpr int MAX_BEATS_IN_AUDIO_BUFFER_PREDICTION = 2048;
        beats.reserve (MAX_BEATS_IN_AUDIO_BUFFER_PREDICTION);
    }

    void prepareToPlay (double sampleRateIn, int samplesPerBlock)
    {
        sampleRate = sampleRateIn;

        mixer.removeAllInputs();

        downbeatAudio->prepareToPlay (samplesPerBlock, sampleRate);
        beatAudio->prepareToPlay (samplesPerBlock, sampleRate);
        mixer.prepareToPlay (samplesPerBlock, sampleRate);
        mixer.addInputSource (downbeatAudio.get(), false);
        mixer.addInputSource (beatAudio.get(), false);

        reset();
    }

    void setBPM (double bpmNew)
    {
        if (! juce::exactlyEqual (bpm, bpmNew))
        {
            bpm = bpmNew;
            sampleCountInCurrentBeat = std::min (sampleCountInCurrentBeat, getSamplesPerBeat()); // ensure valid indexing
            // TODO: Ensure this is correct instead of getSamplesPerBeat() - 1
        }
    }

    // TODO: setTimeSignature() - also handle similarly updating to setBPM where we do not fully resut but we ensure valud state

    void reset()
    {
        sampleCountInCurrentBeat = 0;

        // Prepare first downbeat
        countBeatInMeasure = 0;
        beats.clear();
        beats.push_back ({ 0, true });

        downbeatAudio->setNextReadPosition (0);
        beatAudio->setNextReadPosition (0);
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        // const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        // Determine remaining beat positions for this block
        const int samplesPerBeat = getSamplesPerBeat();
        int offset = samplesPerBeat - sampleCountInCurrentBeat;
        while (offset < numSamples)
        {
            ++countBeatInMeasure %= timeSignature.numerator;
            beats.push_back ({ offset, countBeatInMeasure == 0 });
            offset += samplesPerBeat;
        }

        // If no beat positions in this block, keep counting and output any audio that may be remaining in the beat samples
        if (beats.empty())
        {
            sampleCountInCurrentBeat += numSamples;

            const juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
            info.clearActiveBufferRegion(); // clear before filling via sample->getNextAudioBlock(info), do I really need to do this?
            mixer.getNextAudioBlock (info);
            //beatAudio->getNextAudioBlock (info); // voice 1
            //downbeatAudio->getNextAudioBlock (info); // voice 2
        }
        else // If beat positions in this block, restart audio sample and start outputting it at the expected position
        {
            sampleCountInCurrentBeat = numSamples - beats.back().samplePosition;

            // Output click audio at each beat
            for (const auto& [samplePosition, isDownbeat] : beats)
            {
                const auto & audio = getAudio(isDownbeat);
                audio->setNextReadPosition (0);

                const juce::AudioSourceChannelInfo info (&buffer, samplePosition, numSamples - samplePosition); // allocation?? looks like lightweight struct
                info.clearActiveBufferRegion(); // clear before filling via sample->getNextAudioBlock(info), do I really need to do this?
                //audio->getNextAudioBlock (info);
                mixer.getNextAudioBlock (info);

                // TODO: Handle overlap of audio or double trigger of audio here . . .
                // Something is def wrong right now where I can hear the regular beat audio cut out sometimes.
                // It always happens around the same time in a loop regardless of the BPM.

                // TODO: Fix bug where both audio are played on the first downbeat!!
            }
        }

        // Problem with this is that BOTH audio samples will play on first downbeat
        // const juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
        // //info.clearActiveBufferRegion(); // clear before filling via sample->getNextAudioBlock(info), do I really need to do this?
        // beatAudio->getNextAudioBlock (info); // voice 1
        // downbeatAudio->getNextAudioBlock (info); // voice 2

        beats.clear();

        // TODO: Downbeat audio and beat audio should be ADDED together.
    }

private:
    [[nodiscard]] int getSamplesPerBeat() const { return juce::roundToInt (60.0 / bpm * (4.0 / timeSignature.denominator) * sampleRate); }
    std::unique_ptr<juce::AudioFormatReaderSource>& getAudio (const bool isDownbeat) { return isDownbeat ? downbeatAudio : beatAudio; }

    double bpm = 120;

    struct TimeSignature
    {
        int numerator = 4; // beats per measure (downbeat tracking)
        int denominator = 4; // note which receives beat (4 = quarter, 8 = eighth, 16 = sixteenth)
    };

    TimeSignature timeSignature = { 4, 4 };

    double sampleRate = 0;
    int sampleCountInCurrentBeat = 0;
    int countBeatInMeasure = 0;

    juce::MixerAudioSource mixer;
    std::unique_ptr<juce::AudioFormatReaderSource> downbeatAudio;
    std::unique_ptr<juce::AudioFormatReaderSource> beatAudio;

    struct Beat
    {
        int samplePosition = 0;
        bool isDownbeat = false;
    };
    std::vector<Beat> beats;
};
