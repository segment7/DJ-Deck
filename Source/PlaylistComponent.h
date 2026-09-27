#pragma once
#include <JuceHeader.h>
#include "TrackStore.h"

class PlaylistComponent : public juce::Component,
                          public juce::TableListBoxModel,
                          public juce::Button::Listener,
                          private juce::ChangeListener
{
public:
    /** Creates a library view with a callback for loading a file to deck 0 or 1. */
    PlaylistComponent(TrackStore& store, std::function<void(const juce::File&, int)> load);
    /** Disconnects store notifications. */
    ~PlaylistComponent() override;
    /** Draws the desktop theme background and thin grey panel border. */
    void paint(juce::Graphics& graphics) override;
    /** Arranges the track table above the status and import footer. */
    void resized() override;
    /** Returns the visible track count. */
    int getNumRows() override;
    /** Paints the requested row using selected or normal background colours. */
    void paintRowBackground(juce::Graphics&, int row, int width, int height, bool selected) override;
    /** Paints track metadata for the requested cell. */
    void paintCell(juce::Graphics&, int row, int column, int width, int height, bool selected) override;
    /** Returns the full track name for metadata cells, or empty for buttons and invalid rows. */
    juce::String getCellTooltip(int rowNumber, int columnId) override;
    /** Returns a table-owned action button for the cell, or nullptr for other columns. */
    juce::Component* refreshComponentForCell(int row, int column, bool selected,
                                             juce::Component* existing) override;
    /** Opens multi-file import. */
    void buttonClicked(juce::Button* button) override;
    /** Returns one available selected track ID for an internal drag, or an empty value. */
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override;
    /** Sorts the title or duration column in the requested direction. */
    void sortOrderChanged(int columnId, bool forwards) override;

private:
    enum ColumnId
    {
        titleColumn = 1,
        durationColumn,
        loadDeckAColumn,
        loadDeckBColumn,
        removeColumn
    };

    /** Applies a deferred row action using a stable track ID. */
    void performRowAction(const juce::String& id, int column);
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refresh();
    int sortColumn = titleColumn;
    bool sortAscending = true;
    TrackStore& store;
    std::function<void(const juce::File&, int)> load;
    juce::StringArray ids;
    juce::TextButton importButton{"Add Track(s)..."};
    juce::Label status, trackCount;
    juce::TableListBox table;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistComponent)
};
