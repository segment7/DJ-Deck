/*
  ==============================================================================

    AudioRecorder.h
    Created: 8 Sep 2026 6:02:03pm
    Author:  Rey

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
/** Records stereo blocks using one audio producer and one background file writer.
    Call lifecycle methods on the message thread; capture only from the audio callback.
*/
class AudioRecorder : private juce::Thread
{
public:
    /** Identifies the signal supplied by the main audio callback. */
    enum class Source
    {
        deckA,
        deckB,
        mix,
        microphone
    };

    //==============================================================================
    /** Allocates the fixed stereo recording buffer. */
    AudioRecorder();

    /** Stops capture, joins the writer thread and closes the file. */
    ~AudioRecorder() override;

    /** Starts a new 16-bit WAV at a JUCE-supported rate; returns an error or empty on success. */
    juce::String start(const juce::File& file, double sampleRate, Source source);

    /** Drains and closes a pending recording; returns true for non-empty, successful output. */
    bool finish();

    //==============================================================================
    /** Enqueues the first samples frames from two channels; zero frames are a no-op. */
    void capture(const juce::AudioBuffer<float>& input, int samples);

    /** Stops accepting new blocks without waiting; safe from the audio callback. */
    void requestStop();

    /** Returns whether finalisation is pending; call on the message thread. */
    bool hasRecording() const;

    /** Returns whether blocks are being accepted; safe from the audio callback. */
    bool isCapturing() const;

    /** Returns the selected signal source; safe from the audio callback. */
    Source getSource() const;

    /** Returns captured seconds, limited to 30; call on the message thread. */
    double getSeconds() const;

    /** Returns a recording failure or empty on success; call on the message thread. */
    juce::String getError() const;

private:
    //==============================================================================
    enum class Failure
    {
        none,
        invalidInput,
        bufferOverflow,
        diskWrite
    };

    static constexpr int channelCount = 2;
    static constexpr int fifoCapacity = 131072;
    static constexpr int writeBlockSize = 8192;
    static constexpr double maximumDurationSeconds = 30.0;

    /** Drains queued samples and flushes the WAV on the writing thread. */
    void run() override;

    /** Writes one FIFO batch and returns the number of consumed frames. */
    int writePendingSamples();

    /** Copies a validated block into the FIFO; returns false if capacity is insufficient. */
    bool enqueueSamples(const juce::AudioBuffer<float>& input, int samples);

    /** Records the first failure and stops capture without blocking. */
    void recordFailure(Failure reason);

    /** Returns whether JUCE advertises support for the finite WAV sample rate. */
    static bool isSupportedSampleRate(double rate);

    juce::AbstractFifo fifo{fifoCapacity};
    juce::AudioBuffer<float> audioBuffer{channelCount, fifoCapacity};
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::SpinLock captureLock;

    std::atomic<bool> active{false};
    std::atomic<Failure> failure{Failure::none};
    std::atomic<juce::int64> capturedSamples{0};
    std::atomic<Source> recordingSource{Source::deckA};

    // These values remain fixed between publishing active and joining the writer.
    double sampleRate = 0.0;
    juce::int64 maximumSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)
};
