/*
  ==============================================================================

    LoopPreview.h
    Created: 8 Sep 2026 6:02:12pm
    Author:  Rey

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/** Owns one short loop; load and stop on the message thread, render on one audio thread. */
class LoopPreview
{
public:
    /** Creates an empty preview source. */
    LoopPreview();

    /** Releases the loop after the owner has stopped audio callbacks. */
    ~LoopPreview();

    /** Loads up to 30 seconds at up to 384 kHz; failure preserves the current loop and position. */
    bool load(const juce::File& file);

    /** Stops playback and releases sample memory on the message thread. */
    void stop();

    /** Adds samples frames to output's first two channels; invalid requests leave output unchanged. */
    void addTo(juce::AudioBuffer<float>& output, int samples, double deviceRate);

    /** Returns whether a loop is loaded; call on the message thread. */
    bool isPlaying() const;

private:
    //==============================================================================
    static constexpr int channelCount = 2;
    static constexpr double maximumDurationSeconds = 30.0;
    static constexpr double maximumSampleRate = 384000.0;
    static constexpr double fadeDurationSeconds = 0.003;
    static constexpr float previewGain = 0.7f;

    /** Checks reader metadata before allocating a bounded stereo buffer. */
    static bool canLoad(const juce::AudioFormatReader& reader);

    /** Applies preview gain and short fades to decoded audio on the message thread. */
    static void prepareLoop(juce::AudioBuffer<float>& audio, double rate);

    /** Adds interpolated samples while the caller holds the sample lock. */
    void render(juce::AudioBuffer<float>& output, int samples, double step);

    mutable juce::SpinLock sampleLock;
    juce::AudioBuffer<float> sample;
    double sampleRate = 0.0;
    double position = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopPreview)
};
