#pragma once
#include <JuceHeader.h>
#include "DJAudioPlayer.h"
#include "WaveformDisplay.h"
#include "TrackStore.h"

class DeckGUI : public juce::Component,
                public juce::Button::Listener,
                public juce::Slider::Listener,
                public juce::FileDragAndDropTarget,
                public juce::DragAndDropTarget,
                private juce::Timer
{
public:
    /** Connects the deck view to its player and shared track store. */
    DeckGUI(DJAudioPlayer& player, TrackStore& store, juce::AudioFormatManager& formats,
            juce::AudioThumbnailCache& cache, juce::String name, juce::Colour accent);
    /** Stops UI updates and pending file selection. */
    ~DeckGUI() override;
    /** Draws the deck panel background. */
    void paint(juce::Graphics& graphics) override;
    /** Arranges controls within the deck. */
    void resized() override;
    /** Exposes the existing transport and sliders for the main control area. */
    juce::Component& getPlaybackControls() { return levelControls; }
    /** Positions the detached controls, retaining their original internal arrangement. */
    void layoutPlaybackControls(juce::Rectangle<int> bounds);
    /** Dispatches transport and cue button actions. */
    void buttonClicked(juce::Button* button) override;
    /** Applies changes from the volume or speed slider; ignores other sliders. */
    void sliderValueChanged(juce::Slider* slider) override;
    /** Returns true when the drag contains exactly one file. */
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    /** Loads the single dropped audio file. */
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    /** Shows individual cue editing actions on right-click. */
    void mouseDown(const juce::MouseEvent& event) override;
    /** Loads a file through shared track identity; returns success. */
    bool loadFile(const juce::File& file);
    /** Unloads the specified track if currently selected. */
    void unloadIf(const juce::String& id);
    /** Sets or jumps to cue 0-7; ignores invalid indices and empty decks. */
    void triggerCue(int index);
    /** Accepts a library or sample drag describing one available track ID. */
    bool isInterestedInDragSource(const SourceDetails& details) override;
    /** Loads the dragged track or sample through the shared file loading entry. */
    void itemDropped(const SourceDetails& details) override;

private:
    /** Loads transport artwork into the existing JUCE buttons. */
    void initialiseTransport();
    /** Configures track labels, time editing and action listeners. */
    void initialiseLabels();
    /** Configures editable volume and speed controls with valid ranges. */
    void initialiseSliders();
    void timerCallback() override;
    void refresh();
    void unload();
    void editCue(int index);
    /** Validates edited seconds or mm:ss.xx and seeks without changing playback state. */
    void applyEditedPosition();
    juce::String editingTrackId;
    DJAudioPlayer& player;
    TrackStore& store;
    juce::String trackId;
    juce::Colour accent;
    WaveformDisplay waveform;
    juce::Component levelControls;
    juce::Label trackName, time, duration, volumeLabel, speedLabel, cueHint, status;
    std::array<juce::Component, 4> sliderMargins;
    juce::DrawableButton play{"Play", juce::DrawableButton::ImageFitted};
    juce::DrawableButton stop{"Stop", juce::DrawableButton::ImageFitted};
    std::unique_ptr<juce::Drawable> playIcon, pauseIcon, stopIcon;
    bool showingPause = false;
    juce::TextButton load{"Load"}, eject{"Unload"}, clear{"Clear all"};
    juce::Slider volume, speed;
    std::array<juce::TextButton, 8> cueButtons;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeckGUI)
};
