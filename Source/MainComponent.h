#pragma once
#include <JuceHeader.h>
#include "DeckGUI.h"
#include "PlaylistComponent.h"
#include "SamplerComponent.h"
#include <atomic>

class MainComponent : public juce::AudioAppComponent,
                      public juce::DragAndDropContainer,
                      private juce::Timer,
                      private juce::KeyListener,
                      private juce::ComponentListener
{
public:
    /** Creates the interface; opens services after the window becomes visible. */
    MainComponent();
    /** Finalises recordings and closes audio before destroying sources. */
    ~MainComponent() override;
    /** Prepares players and fixed-size mixing buffers for the device. */
    void prepareToPlay(int blockSize, double sampleRate) override;
    /** Captures input, renders each deck once, records and mixes preview. */
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& output) override;
    /** Stops capture and releases device-dependent player buffers. */
    void releaseResources() override;
    /** Paints the application background. */
    void paint(juce::Graphics& graphics) override;
    /** Arranges decks above three adjustable library, playback and loop columns. */
    void resized() override;

private:
    /** Captures the selected rendered source before sample preview is mixed in. */
    void captureRecording(int samples);
    /** Loads saved metadata and opens output once after the initial window display. */
    void initialiseServices();
    /** Enables input on demand; returns false if no microphone can be opened. */
    bool enableMicrophone();
    /** Creates audio settings only when their tab is first shown. */
    void componentVisibilityChanged(juce::Component& component) override;
    /** Cancels non-left mouse presses on descendant buttons. */
    void mouseDown(const juce::MouseEvent& event) override;
    /** Updates settings when a tab receives new bounds. */
    void componentMovedOrResized(juce::Component& component, bool moved, bool resized) override;
    /** Fits settings to their tabs, retaining vertical audio scrolling. */
    void layoutSettings();
    /** Keeps A and B playback controls side by side when their column is resized. */
    void layoutPlaybackControls();
    /** Consumes cue key presses; state changes perform each action once. */
    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
    /** Routes number keys to A, or Shift-number keys to B, without repeats. */
    bool keyStateChanged(bool isKeyDown, juce::Component* origin) override;
    /** Returns whether keyboard cues may run outside text entry and modal dialogs. */
    bool canTriggerKeyboardCue() const;
    std::array<bool, 8> heldCueKeys{};
    void timerCallback() override;
    void chooseProject();
    juce::LookAndFeel_V4 theme;
    TrackStore store;
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache cache{64};
    DJAudioPlayer playerA{formats}, playerB{formats};
    AudioRecorder recorder;
    LoopPreview preview;
    std::atomic<double> deviceRate{0};
    bool started = false;
    juce::AudioBuffer<float> bufferA, bufferB, mixBuffer, inputBuffer;
    DeckGUI deckA{playerA, store, formats, cache, "DECK A", juce::Colours::skyblue};
    DeckGUI deckB{playerB, store, formats, cache, "DECK B", juce::Colours::plum};
    PlaylistComponent library;
    SamplerComponent sampler;
    juce::Component controlsPanel, projectPanel;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioPanel;
    juce::Viewport audioViewport;
    juce::StretchableLayoutManager columnLayout;
    juce::StretchableLayoutResizerBar columnDivider{&columnLayout, 1, true};
    juce::StretchableLayoutResizerBar rightColumnDivider{&columnLayout, 3, true};
    juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    juce::TabbedComponent rightTabs{juce::TabbedButtonBar::TabsAtTop};
    juce::Label status;
    juce::TextButton projectFolder{"Choose project folder"};
    juce::TooltipWindow tooltips{this, 700};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String startupMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
