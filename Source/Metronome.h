
#pragma once

#include "TimeSignature.h"
#include "BinaryData.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <span>

class Metronome
{
public:
    Metronome()
    {
        loadAudioSamples();

        // Be able to handle enough beat positions per buffer without reallocation in audio callback.
        static constexpr int MAX_BEATS_IN_AUDIO_BUFFER_PREDICTION = 2048;
        beats.reserve (MAX_BEATS_IN_AUDIO_BUFFER_PREDICTION);
    }

    void prepareToPlay (const double sampleRateIn, const int samplesPerBlock)
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

    void reset()
    {
        countSampleInCurrentBeat = 0;

        // Prepare first downbeat
        countBeatInMeasure = 0;
        beats.clear();
        beats.push_back ({ 0, true });

        // Set audio clips to be "stopped" until its read position is set to 0 the first time.
        downbeatAudio->setNextReadPosition (downbeatAudio->getTotalLength() - 1);
        beatAudio->setNextReadPosition (beatAudio->getTotalLength() - 1);
    }

    void setBPM (const double bpmNew)
    {
        if (! juce::exactlyEqual (bpm, bpmNew))
        {
            bpm = bpmNew;

            /// When user changes BPM, keep counting, but never surpass the current beat length, which could cause BAD_ACCESS in `process()`.
            ///     Alternatively, we could set `countSampleInCurrentBeat = 0`, but this causes the beat to
            ///     hit every time a new BPM change is detected, so when user drags slider, it sounds horrible.
            countSampleInCurrentBeat = std::min (countSampleInCurrentBeat, getSamplesPerBeat());
        }
    }

    void setTimeSignature (const TimeSignature timeSignatureNew)
    {
        if (timeSignature != timeSignatureNew)
        {
            timeSignature = timeSignatureNew;
            countBeatInMeasure = std::min (countBeatInMeasure, timeSignature.beatsPerMeasure - 1);
        }
    }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamplesInBuffer = buffer.getNumSamples();

        // Find beat positions in current block
        const int samplesPerBeat = getSamplesPerBeat();
        for (int offset = std::max (samplesPerBeat - countSampleInCurrentBeat, 0); offset < numSamplesInBuffer; offset += samplesPerBeat)
        {
            ++countBeatInMeasure %= timeSignature.beatsPerMeasure; // consider using if statement instead of modulo for speed
            beats.push_back ({ offset, countBeatInMeasure == 0 });
        }

        // If no beat positions in this block, keep counting and output any audio that may be remaining in the beat samples
        if (beats.empty())
        {
            /// Alternatively, we could increase `countSampleInCurrentBeat` like:
            /// ```cpp
            /// countSampleInCurrentBeat = (numSamplesInBuffer + countSampleInCurrentBeat) % samplesPerBeat;
            /// ```
            // But `%` modulo relies on division which is more computationally heavy.
            // We can obtain the same result heuristically with this if statement, without division.
            countSampleInCurrentBeat += numSamplesInBuffer;

            const juce::AudioSourceChannelInfo info (&buffer, 0, numSamplesInBuffer);
            mixer.getNextAudioBlock (info);
        }
        else // If beat positions in this block, restart audio sample and start outputting it at the expected position
        {
            countSampleInCurrentBeat = numSamplesInBuffer - beats.back().samplePosition;

            // Output click audio starting at each beat
            for (size_t beatIndex = 0; beatIndex < beats.size(); ++beatIndex)
            {
                const auto& [beatPosition, isDownbeat] = beats[beatIndex];
                const int nextBeatPosition = beatIndex + 1 < beats.size() ? beats[beatIndex + 1].samplePosition : numSamplesInBuffer;

                getAudioForBeat (isDownbeat)->setNextReadPosition (0); // restart playback position of specific audio clip

                const juce::AudioSourceChannelInfo info (&buffer, beatPosition, nextBeatPosition - beatPosition);
                mixer.getNextAudioBlock (info);
            }
        }

        beats.clear();
    }

private:
    void loadAudioSamples()
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        auto makeReaderSource = [&formatManager] (const char* bytes, const int size)
        {
            auto* reader = formatManager.createReaderFor (std::make_unique<juce::MemoryInputStream> (bytes, static_cast<size_t> (size), false));
            // TODO: Handle audio file not found instead of crashing.
            jassert(reader != nullptr);
            return std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        };

        downbeatAudio = makeReaderSource (BinaryData::clickdownbeat_wav, BinaryData::clickdownbeat_wavSize);
        beatAudio = makeReaderSource (BinaryData::clickbeat_wav, BinaryData::clickbeat_wavSize);
    }

    [[nodiscard]] int getSamplesPerBeat() const { return juce::roundToInt (60.0 / bpm * (4.0 / timeSignature.denominator) * sampleRate); }
    std::unique_ptr<juce::AudioFormatReaderSource>& getAudioForBeat (const bool isDownbeat) { return isDownbeat ? downbeatAudio : beatAudio; }

    double bpm = 120;

    TimeSignature timeSignature = { { 4 }, { 4 } };

    double sampleRate = 0;
    int countSampleInCurrentBeat = 0; // starts counting at 1, so 1 means 1 sample
    int countBeatInMeasure = 0; // starts counting at 0, so 0 is the downbeat

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
