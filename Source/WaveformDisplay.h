#pragma once
#include <JuceHeader.h>
#include <array>

class WaveformDisplay : public juce::Component, private juce::ChangeListener
{
public:
    /** Creates a waveform using the shared registry and thumbnail cache. */
    WaveformDisplay(juce::AudioFormatManager& formats, juce::AudioThumbnailCache& cache);
    /** Unregisters thumbnail notifications. */
    ~WaveformDisplay() override;
    /** Draws the waveform, cue markers and playhead. */
    void paint(juce::Graphics& graphics) override;
    /** Loads a thumbnail, or clears the display for an empty file. */
    void loadFile(const juce::File& file);
    /** Updates normalised progress, cue seconds and accent colour. */
    void update(double position, const std::array<double, 8>& cues, juce::Colour accent);
    /** Sets the callback receiving a normalised position when the waveform is dragged. */
    void setSeekCallback(std::function<void(double)> callback);
    /** Seeks on a left click when a waveform is loaded. */
    void mouseDown(const juce::MouseEvent& event) override;
    /** Seeks while dragging within or beyond the waveform bounds. */
    void mouseDrag(const juce::MouseEvent& event) override;

private:
    std::function<void(double)> seek;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    juce::AudioThumbnail thumbnail;
    double position = 0;
    std::array<double, 8> cues{-1, -1, -1, -1, -1, -1, -1, -1};
    juce::Colour accent{0xff69b9d5};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
