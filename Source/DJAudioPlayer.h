#pragma once
#include <JuceHeader.h>

class DJAudioPlayer : public juce::AudioSource
{
public:
    /** Creates a player using the shared format registry. */
    explicit DJAudioPlayer(juce::AudioFormatManager& formats);
    /** Detaches the reader before releasing owned sources. */
    ~DJAudioPlayer() override;
    /** Prepares resampling for the device block size and sample rate. */
    void prepareToPlay(int blockSize, double sampleRate) override;
    /** Renders the next block into the supplied output region. */
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& output) override;
    /** Releases device-dependent buffers. */
    void releaseResources() override;
    /** Loads a readable file with looping enabled; preserves the current track on failure. */
    bool loadFile(const juce::File& file);
    /** Stops playback and releases the loaded track. */
    void unload();
    /** Clamps finite linear gain to 0-1; ignores non-finite input. */
    void setGain(double gain);
    /** Clamps finite speed to 0.25-2; ignores non-finite input. */
    void setSpeed(double ratio);
    /** Seeks to a clamped position in seconds. */
    void setPosition(double seconds);
    /** Seeks to a normalised position in the range 0-1. */
    void setPositionRelative(double position);
    /** Starts the loaded track, restarting if it has ended. */
    void start();
    /** Pauses without resetting the position. */
    void pause();
    /** Stops playback and returns to the beginning. */
    void stop();
    /** Returns duration in seconds, or 0 if empty. */
    double getDurationSeconds() const;
    /** Returns the current position in seconds. */
    double getPositionSeconds() const;
    /** Returns normalised progress, or 0 if empty. */
    double getPositionRelative() const;
    /** Returns whether a track is loaded. */
    bool isLoaded() const;
    /** Returns whether the transport is playing. */
    bool isPlaying() const;

private:
    juce::AudioFormatManager& formats;
    juce::TimeSliceThread readThread{"Track read ahead"};
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    juce::AudioTransportSource transport;
    juce::ResamplingAudioSource resampler{&transport, false, 2};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DJAudioPlayer)
};
