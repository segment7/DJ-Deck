/*
  ==============================================================================

    AudioRecorder.cpp
    Created: 8 Sep 2026 6:02:03pm
    Author:  Rey

  ==============================================================================
*/

#include "AudioRecorder.h"
#include <cmath>

//==============================================================================
AudioRecorder::AudioRecorder() : juce::Thread("WAV recording writer")
{
}

AudioRecorder::~AudioRecorder()
{
    finish();
}

juce::String AudioRecorder::start(const juce::File& file, double rate, Source selected)
{
    if (hasRecording())
        return "Finish the current recording first.";

    if (!isSupportedSampleRate(rate))
        return "No supported audio sample rate. Check Audio settings.";

    if (selected < Source::deckA || selected > Source::microphone)
        return "Choose a valid recording source.";

    if (file.exists())
        return "The recording file already exists. Choose a new file.";

    auto stream = file.createOutputStream();
    if (!stream || stream->failedToOpen())
        return "Cannot create the WAV file.";

    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> output(std::move(stream));
    const auto options = juce::AudioFormatWriterOptions()
                             .withSampleRate(rate)
                             .withNumChannels(channelCount)
                             .withBitsPerSample(16);

    writer = format.createWriterFor(output, options);
    if (!writer)
        return "Cannot create the WAV writer.";

    sampleRate = rate;
    maximumSamples = static_cast<juce::int64>(rate * maximumDurationSeconds);
    recordingSource.store(selected);
    capturedSamples.store(0);
    failure.store(Failure::none);
    fifo.reset();

    if (!startThread())
    {
        writer.reset();
        return "Cannot start recording thread.";
    }

    // Publish the prepared session before the audio callback reads its settings.
    active.store(true, std::memory_order_release);
    return {};
}

bool AudioRecorder::finish()
{
    if (!hasRecording())
        return false;

    {
        // Wait only on the message thread for any in-flight producer to finish.
        const juce::SpinLock::ScopedLockType lock(captureLock);
        active.store(false, std::memory_order_release);
    }

    signalThreadShouldExit();
    notify();
    waitForThreadToExit(-1);

    // run() has completed all writes and the final flush before ownership is released.
    writer.reset();
    return failure.load() == Failure::none && capturedSamples.load() > 0;
}

//==============================================================================
void AudioRecorder::capture(const juce::AudioBuffer<float>& input, int samples)
{
    const juce::SpinLock::ScopedTryLockType lock(captureLock);
    if (!lock.isLocked() || !active.load(std::memory_order_acquire))
        return;

    if (samples == 0)
        return;

    if (samples < 0 || samples > input.getNumSamples() || input.getNumChannels() < channelCount)
    {
        recordFailure(Failure::invalidInput);
        return;
    }

    const auto remaining = maximumSamples - capturedSamples.load();
    const int framesToCopy = static_cast<int>(juce::jmin(static_cast<juce::int64>(samples), remaining));

    if (framesToCopy <= 0)
    {
        requestStop();
        return;
    }

    if (!enqueueSamples(input, framesToCopy))
    {
        recordFailure(Failure::bufferOverflow);
        return;
    }

    const auto total = capturedSamples.fetch_add(framesToCopy) + framesToCopy;
    if (total >= maximumSamples)
        requestStop();
}

void AudioRecorder::requestStop()
{
    active.store(false, std::memory_order_release);
}

bool AudioRecorder::hasRecording() const
{
    return writer != nullptr;
}

bool AudioRecorder::isCapturing() const
{
    return active.load(std::memory_order_acquire);
}

AudioRecorder::Source AudioRecorder::getSource() const
{
    return recordingSource.load();
}

double AudioRecorder::getSeconds() const
{
    return sampleRate > 0.0 ? static_cast<double>(capturedSamples.load()) / sampleRate : 0.0;
}

juce::String AudioRecorder::getError() const
{
    switch (failure.load())
    {
    case Failure::invalidInput:
        return "Recording stopped: invalid stereo input buffer.";
    case Failure::bufferOverflow:
        return "Recording stopped: the disk writer could not keep up.";
    case Failure::diskWrite:
        return "Recording failed: unable to write or flush the WAV file.";
    case Failure::none:
        break;
    }

    return capturedSamples.load() == 0 ? "No audio captured." : juce::String();
}

//==============================================================================
void AudioRecorder::run()
{
    // A stop request ends the thread only after all accepted samples are consumed.
    while (!threadShouldExit() || fifo.getNumReady() > 0)
    {
        if (writePendingSamples() == 0)
            wait(2);
    }

    if (!writer->flush())
        recordFailure(Failure::diskWrite);
}

int AudioRecorder::writePendingSamples()
{
    int firstStart, firstCount, secondStart, secondCount;
    fifo.prepareToRead(writeBlockSize, firstStart, firstCount, secondStart, secondCount);

    // After a disk error, release queued slots so shutdown can still complete.
    if (failure.load() != Failure::diskWrite)
    {
        const bool firstWritten =
            firstCount == 0 || writer->writeFromAudioSampleBuffer(audioBuffer, firstStart, firstCount);
        const bool secondWritten =
            firstWritten &&
            (secondCount == 0 || writer->writeFromAudioSampleBuffer(audioBuffer, secondStart, secondCount));

        if (!firstWritten || !secondWritten)
            recordFailure(Failure::diskWrite);
    }

    const int consumed = firstCount + secondCount;
    fifo.finishedRead(consumed);
    return consumed;
}

bool AudioRecorder::enqueueSamples(const juce::AudioBuffer<float>& input, int samples)
{
    int firstStart, firstCount, secondStart, secondCount;
    fifo.prepareToWrite(samples, firstStart, firstCount, secondStart, secondCount);

    if (firstCount + secondCount < samples)
        return false;

    // A circular-buffer wrap splits the copy into at most two regions.
    for (int channel = 0; channel < channelCount; ++channel)
    {
        if (firstCount > 0)
            audioBuffer.copyFrom(channel, firstStart, input, channel, 0, firstCount);

        if (secondCount > 0)
            audioBuffer.copyFrom(channel, secondStart, input, channel, firstCount, secondCount);
    }

    fifo.finishedWrite(samples);
    return true;
}

void AudioRecorder::recordFailure(Failure reason)
{
    auto expected = Failure::none;
    failure.compare_exchange_strong(expected, reason);
    requestStop();
}

bool AudioRecorder::isSupportedSampleRate(double rate)
{
    if (!std::isfinite(rate))
        return false;

    juce::WavAudioFormat format;
    for (const auto supported : format.getPossibleSampleRates())
    {
        if (rate == static_cast<double>(supported))
            return true;
    }

    return false;
}
