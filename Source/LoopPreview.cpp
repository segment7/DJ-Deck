/*
  ==============================================================================

    LoopPreview.cpp
    Created: 8 Sep 2026 6:02:12pm
    Author:  Rey

  ==============================================================================
*/

#include "LoopPreview.h"
#include <cmath>

//==============================================================================
LoopPreview::LoopPreview() = default;

LoopPreview::~LoopPreview()
{
    stop();
}

bool LoopPreview::load(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));

    if (!reader || !canLoad(*reader))
        return false;

    juce::AudioBuffer<float> next(channelCount, static_cast<int>(reader->lengthInSamples));

    // JUCE duplicates a mono reader into both channels of a stereo target.
    if (!reader->read(&next, 0, next.getNumSamples(), 0, true, true))
        return false;

    prepareLoop(next, reader->sampleRate);

    {
        const juce::SpinLock::ScopedLockType guard(sampleLock);
        std::swap(sample, next);
        sampleRate = reader->sampleRate;
        position = 0.0;
    }

    // The previous buffer is freed here, after the lock has been released.
    return true;
}

void LoopPreview::stop()
{
    juce::AudioBuffer<float> previous;

    {
        const juce::SpinLock::ScopedLockType guard(sampleLock);
        std::swap(sample, previous);
        sampleRate = 0.0;
        position = 0.0;
    }
}

void LoopPreview::addTo(juce::AudioBuffer<float>& output, int samples, double deviceRate)
{
    if (samples <= 0 || samples > output.getNumSamples() || output.getNumChannels() < channelCount ||
        !std::isfinite(deviceRate) || deviceRate <= 0.0)
        return;

    const juce::SpinLock::ScopedTryLockType guard(sampleLock);
    if (!guard.isLocked() || sample.getNumSamples() < 2)
        return;

    const double step = sampleRate / deviceRate;
    if (!std::isfinite(step) || step <= 0.0)
        return;

    render(output, samples, step);
}

bool LoopPreview::isPlaying() const
{
    const juce::SpinLock::ScopedLockType guard(sampleLock);
    return sample.getNumSamples() > 1;
}

//==============================================================================
bool LoopPreview::canLoad(const juce::AudioFormatReader& reader)
{
    return std::isfinite(reader.sampleRate) && reader.sampleRate > 0.0 &&
           reader.sampleRate <= maximumSampleRate && reader.numChannels > 0 && reader.lengthInSamples > 1 &&
           reader.lengthInSamples <= reader.sampleRate * maximumDurationSeconds;
}

void LoopPreview::prepareLoop(juce::AudioBuffer<float>& audio, double rate)
{
    const int length = audio.getNumSamples();
    const int fadeFrames = juce::jlimit(1, length / 2, juce::roundToInt(rate * fadeDurationSeconds));

    audio.applyGain(previewGain);
    audio.applyGainRamp(0, fadeFrames, 0.0f, 1.0f);
    audio.applyGainRamp(length - fadeFrames, fadeFrames, 1.0f, 0.0f);

    // JUCE's ramp approaches its end gain; explicitly silence the final frame.
    audio.clear(length - 1, 1);
}

void LoopPreview::render(juce::AudioBuffer<float>& output, int samples, double step)
{
    const int length = sample.getNumSamples();

    // Reduce extreme finite steps before addition to keep the playhead bounded.
    const double loopStep = std::fmod(step, static_cast<double>(length));

    for (int frame = 0; frame < samples; ++frame)
    {
        const int index = static_cast<int>(position);
        const int nextIndex = (index + 1) % length;
        const float fraction = static_cast<float>(position - index);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            const float current = sample.getSample(channel, index);
            const float next = sample.getSample(channel, nextIndex);
            const float interpolated = current + (next - current) * fraction;
            output.addSample(channel, frame, interpolated);
        }

        position = std::fmod(position + loopStep, static_cast<double>(length));
    }
}
