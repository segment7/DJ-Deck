/*
  ==============================================================================

    SamplerComponent.h
    Created: 8 Sep 2026 6:20:03pm
    Author:  Rey

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "TrackStore.h"
#include "AudioRecorder.h"
#include "LoopPreview.h"

//==============================================================================
/** Presents recording and preview controls; all UI actions run on the message thread. */
class SamplerComponent : public juce::Component,
                         public juce::TableListBoxModel,
                         public juce::Button::Listener,
                         private juce::Timer,
                         private juce::ChangeListener
{
public:
    /** Binds services, rate queries, on-demand microphone setup and deck unloading. */
    SamplerComponent(TrackStore& store, AudioRecorder& recorder, LoopPreview& preview,
                     std::function<double()> sampleRate, std::function<bool()> enableMicrophone,
                     std::function<bool()> microphoneAvailable, std::function<bool(int)> deckLoaded,
                     std::function<void(const juce::String&)> unload);

    /** Finalises recording, stops preview and disconnects listeners before releasing controls. */
    ~SamplerComponent() override;

    /** Draws the sampler background. */
    void paint(juce::Graphics& graphics) override;

    /** Arranges source selection, transport, feedback and sample rows. */
    void resized() override;

    /** Returns the number of listed sample IDs. */
    int getNumRows() override;

    /** Paints the requested row using selected or normal background colours. */
    void paintRowBackground(juce::Graphics& graphics, int row, int width, int height, bool selected) override;

    /** Paints sample metadata in the requested cell; invalid rows are ignored. */
    void paintCell(juce::Graphics& graphics, int row, int column, int width, int height,
                   bool selected) override;

    /** Returns the full sample name for metadata cells, or empty for buttons and invalid rows. */
    juce::String getCellTooltip(int rowNumber, int columnId) override;

    /** Returns a table-owned action button bound to a sample ID, or nullptr for other cells. */
    juce::Component* refreshComponentForCell(int row, int column, bool selected,
                                             juce::Component* existing) override;

    /** Routes recognised transport buttons to their actions. */
    void buttonClicked(juce::Button* button) override;

    /** Finalises a pending recording and lists its WAV when valid; otherwise reports failure. */
    void finishRecording();
    /** Sorts sample names or durations in the requested direction. */
    void sortOrderChanged(int columnId, bool forwards) override;
    /** Returns one available sample ID for dragging to a deck, or an empty value. */
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override;

private:
    //==============================================================================
    enum Column
    {
        sampleColumn = 1,
        durationColumn,
        previewColumn,
        deleteColumn
    };

    /** Updates elapsed time and finalises a recording stopped by the audio service. */
    void timerCallback() override;

    /** Reloads displayed IDs when the shared store changes. */
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    /** Configures JUCE controls and their initial labels. */
    void initialiseControls();

    /** Starts recording after validating the chosen source and service state. */
    void startRecording();

    /** Toggles the current sample or starts another; failure preserves the previous preview. */
    void previewSample(const juce::String& id);

    /** Stops preview and clears its row indicator. */
    void stopSamplePreview();

    /** Stops users of the sample before requesting its file and record removal. */
    void deleteSample(const juce::String& id);

    /** Updates IDs, row components and the empty-list message. */
    void refreshSampleList();
    int sortColumn = sampleColumn;
    bool sortAscending = true;

    /** Updates enabled states immediately after actions and during polling. */
    void updateControls();

    /** Sets visible feedback and its full-text tooltip. */
    void showStatus(const juce::String& message);

    /** Returns a read-only record for a visible row, or nullptr. */
    const TrackStore::Track* trackForRow(int row) const;

    TrackStore& store;
    AudioRecorder& recorder;
    LoopPreview& preview;
    std::function<double()> sampleRate;
    std::function<bool()> enableMicrophone;
    std::function<bool()> microphoneAvailable;
    std::function<bool(int)> deckLoaded;
    std::function<void(const juce::String&)> unload;

    juce::ComboBox source;
    juce::Label sourceLabel, status, emptyList;
    juce::TextButton record{"Record"};
    juce::TextButton finish{"Stop & save"};
    juce::TableListBox table;
    std::unique_ptr<juce::Drawable> playIcon, stopIcon;

    juce::StringArray ids;
    juce::String previewId;
    juce::String recordingSourceName;
    juce::File recordingFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerComponent)
};
