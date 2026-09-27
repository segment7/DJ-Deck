// Extended from the template by Matthew.
#include "DJAudioPlayer.h"
#include <cmath>

DJAudioPlayer::DJAudioPlayer(juce::AudioFormatManager& manager) : formats(manager)
{
}
DJAudioPlayer::~DJAudioPlayer()
{
    unload();
    readThread.stopThread(3000);
}
void DJAudioPlayer::prepareToPlay(int size, double rate)
{
    resampler.prepareToPlay(size, rate);
}
void DJAudioPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& output)
{
    resampler.getNextAudioBlock(output);
}
void DJAudioPlayer::releaseResources()
{
    resampler.releaseResources();
}
bool DJAudioPlayer::loadFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> next(formats.createReaderFor(file));
    if (!next || next->sampleRate <= 0 || next->lengthInSamples <= 0)
        return false;
    if (!readThread.isThreadRunning() && !readThread.startThread())
        return false;
    const auto rate = next->sampleRate;
    auto source = std::make_unique<juce::AudioFormatReaderSource>(next.release(), true);
    source->setLooping(true);
    transport.stop();
    transport.setSource(source.get(), 32768, &readThread, rate);
    reader = std::move(source);
    transport.setPosition(0);
    resampler.flushBuffers();
    return true;
}
void DJAudioPlayer::unload()
{
    transport.stop();
    transport.setSource(nullptr);
    reader.reset();
}
void DJAudioPlayer::setGain(double value)
{
    if (std::isfinite(value))
        transport.setGain((float)juce::jlimit(0.0, 1.0, value));
}
void DJAudioPlayer::setSpeed(double ratio)
{
    if (std::isfinite(ratio))
        resampler.setResamplingRatio(juce::jlimit(0.25, 2.0, ratio));
}
void DJAudioPlayer::setPosition(double seconds)
{
    if (isLoaded() && std::isfinite(seconds))
        transport.setPosition(juce::jlimit(0.0, getDurationSeconds(), seconds));
}
void DJAudioPlayer::setPositionRelative(double position)
{
    if (std::isfinite(position))
        setPosition(juce::jlimit(0.0, 1.0, position) * getDurationSeconds());
}
void DJAudioPlayer::start()
{
    if (!isLoaded())
        return;
    if (getPositionSeconds() >= getDurationSeconds())
        setPosition(0);
    transport.start();
}
void DJAudioPlayer::pause()
{
    transport.stop();
}
void DJAudioPlayer::stop()
{
    pause();
    setPosition(0);
}
double DJAudioPlayer::getDurationSeconds() const
{
    return transport.getLengthInSeconds();
}
double DJAudioPlayer::getPositionSeconds() const
{
    return transport.getCurrentPosition();
}
double DJAudioPlayer::getPositionRelative() const
{
    const auto length = getDurationSeconds();
    return length > 0 ? juce::jlimit(0.0, 1.0, getPositionSeconds() / length) : 0;
}
bool DJAudioPlayer::isLoaded() const
{
    return reader != nullptr;
}
bool DJAudioPlayer::isPlaying() const
{
    return transport.isPlaying();
}
