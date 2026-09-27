// Extended from the template by Matthew.
#include "WaveformDisplay.h"

WaveformDisplay::WaveformDisplay(juce::AudioFormatManager& formats, juce::AudioThumbnailCache& cache)
    : thumbnail(512, formats, cache)
{
    thumbnail.addChangeListener(this);
}
WaveformDisplay::~WaveformDisplay()
{
    thumbnail.removeChangeListener(this);
}
void WaveformDisplay::setSeekCallback(std::function<void(double)> callback)
{
    seek = std::move(callback);
}
void WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    mouseDrag(event);
}
void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (event.mods.isLeftButtonDown() && thumbnail.getTotalLength() > 0 && getWidth() > 0 && seek)
        seek(juce::jlimit(0.0, 1.0, static_cast<double>(event.position.x) / getWidth()));
}
void WaveformDisplay::loadFile(const juce::File& file)
{
    thumbnail.clear();
    position = 0;
    if (file.existsAsFile())
        thumbnail.setSource(new juce::FileInputSource(file));
    repaint();
}
void WaveformDisplay::update(double pos, const std::array<double, 8>& marks, juce::Colour colour)
{
    if (position == pos && cues == marks && accent == colour)
        return;
    position = pos;
    cues = marks;
    accent = colour;
    repaint();
}
void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster*)
{
    repaint();
}
void WaveformDisplay::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
    const auto length = thumbnail.getTotalLength();
    if (length <= 0)
    {
        g.setColour(juce::Colour(0xff919ba9));
        g.drawText("Drag and drop a track here (from the library or sys folder)", getLocalBounds(),
                   juce::Justification::centred);
        return;
    }
    g.setColour(accent.withAlpha(0.75f));
    thumbnail.drawChannels(g, getLocalBounds().reduced(0, 18), 0, length, 0.85f);
    for (size_t i = 0; i < cues.size(); ++i)
        if (cues[i] >= 0)
        {
            const auto x = (float)(cues[i] / length * getWidth());
            g.setColour(accent.withAlpha(0.55f));
            g.drawVerticalLine((int)x, 18.0f, (float)getHeight());
            g.setColour(accent);
            g.drawText(juce::String((int)i + 1), juce::jlimit(0, juce::jmax(0, getWidth() - 18), (int)x), 0,
                       18, 18, juce::Justification::centred);
        }
    g.setColour(juce::Colours::white);
    g.drawVerticalLine(juce::jlimit(0, getWidth() - 1, (int)(position * getWidth())), 0, (float)getHeight());
}
