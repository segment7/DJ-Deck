#include "PlaylistComponent.h"

PlaylistComponent::PlaylistComponent(TrackStore& data, std::function<void(const juce::File&, int)> callback)
    : store(data), load(std::move(callback))
{
    addAndMakeVisible(importButton);
    importButton.addListener(this);
    addAndMakeVisible(status);
    addAndMakeVisible(table);
    addAndMakeVisible(trackCount);
    trackCount.setJustificationType(juce::Justification::centredRight);
    table.setModel(this);
    table.setRowHeight(32);
    table.getHeader().addColumn("Title", titleColumn, 220, 100);
    table.getHeader().addColumn("Time", durationColumn, 70, 50);
    table.getHeader().addColumn("A", loadDeckAColumn, 32, 32, -1, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn("B", loadDeckBColumn, 32, 32, -1, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn("Library", removeColumn, 70, 60, -1, juce::TableHeaderComponent::notSortable);
    store.addChangeListener(this);
    refresh();
    table.getHeader().setSortColumnId(titleColumn, true);
}
PlaylistComponent::~PlaylistComponent()
{
    store.removeChangeListener(this);
    importButton.removeListener(this);
    table.setModel(nullptr);
}
void PlaylistComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    graphics.setColour(juce::Colours::grey);
    graphics.drawRect(getLocalBounds(), 1);
}
void PlaylistComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto toolbar = area.removeFromBottom(40);
    importButton.setBounds(toolbar.removeFromLeft(130).reduced(0, 4));
    trackCount.setBounds(toolbar);
    status.setVisible(status.getText().isNotEmpty());
    status.setBounds(status.isVisible() ? area.removeFromBottom(40) : juce::Rectangle<int>());
    table.setBounds(area);
    table.getHeader().setColumnWidth(titleColumn, juce::jmax(100, table.getWidth() - 230));
}
int PlaylistComponent::getNumRows()
{
    return ids.size();
}
void PlaylistComponent::paintRowBackground(juce::Graphics& g, int row, int, int, bool selected)
{
    juce::ignoreUnused(row);
    g.fillAll(selected ? juce::Colours::lightsalmon : juce::Colours::transparentWhite);
}
void PlaylistComponent::paintCell(juce::Graphics& g, int row, int column, int width, int height,
                                  bool selected)
{
    if (!juce::isPositiveAndBelow(row, ids.size()))
        return;
    if (const auto* track = store.find(ids[row]))
    {
        const bool exists = store.fileFor(*track).existsAsFile();
        auto textColour = exists ? juce::Colours::white : juce::Colours::lightsalmon;
        if (selected)
            textColour = juce::Colours::black;
        g.setColour(textColour);
        if (column == titleColumn)
            g.drawText((exists ? "" : "[Missing] ") + track->name, 10, 0, width - 20, height,
                       juce::Justification::centredLeft);
        if (column == durationColumn)
        {
            const int seconds = (int)track->duration;
            g.drawText(juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2), 8, 0,
                       width - 16, height, juce::Justification::centredLeft);
        }
    }
}
juce::String PlaylistComponent::getCellTooltip(int rowNumber, int columnId)
{
    if ((columnId != titleColumn && columnId != durationColumn) ||
        !juce::isPositiveAndBelow(rowNumber, ids.size()))
        return {};

    if (const auto* track = store.find(ids[rowNumber]))
        return track->name;

    return {};
}

juce::Component* PlaylistComponent::refreshComponentForCell(int row, int column, bool,
                                                            juce::Component* existing)
{
    if ((column != loadDeckAColumn && column != loadDeckBColumn && column != removeColumn) ||
        !juce::isPositiveAndBelow(row, ids.size()))
    {
        delete existing;
        return nullptr;
    }
    auto* button = dynamic_cast<juce::TextButton*>(existing);
    if (!button)
    {
        delete existing;
        button = new juce::TextButton();
    }
    const auto id = ids[row];
    juce::String action;
    switch (column)
    {
    case loadDeckAColumn:
        button->setButtonText("+");
        action = "Load to Deck A";
        break;
    case loadDeckBColumn:
        button->setButtonText("+");
        action = "Load to Deck B";
        break;
    case removeColumn:
        button->setButtonText("Remove");
        action = "Remove from library";
        break;
    default:
        jassertfalse;
        delete button;
        return nullptr;
    }
    button->setTitle(action);
    const auto* track = store.find(id);
    button->setEnabled(track && (column == removeColumn || store.fileFor(*track).existsAsFile()));
    button->setTooltip(action);
    button->onClick = [safe = juce::Component::SafePointer<PlaylistComponent>(this), id, column]
    {
        // Let the button callback return before its table can rebuild the row.
        juce::MessageManager::callAsync(
            [safe, id, column]
            {
                if (safe)
                    safe->performRowAction(id, column);
            });
    };
    return button;
}
void PlaylistComponent::performRowAction(const juce::String& id, int column)
{
    if (column == removeColumn)
        store.remove(id);
    else if (column == loadDeckAColumn || column == loadDeckBColumn)
    {
        if (const auto* track = store.find(id))
            load(store.fileFor(*track), column == loadDeckAColumn ? 0 : 1);
    }
    else
        return;

    refresh();
}

void PlaylistComponent::buttonClicked(juce::Button* button)
{
    if (button != &importButton || !store.isReady())
        return;
    chooser = std::make_unique<juce::FileChooser>("Import audio tracks", juce::File(),
                                                  "*.wav;*.mp3;*.aiff;*.flac;*.ogg");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::canSelectMultipleItems,
        [safe = juce::Component::SafePointer<PlaylistComponent>(this)](const juce::FileChooser& selected)
        {
            if (!safe || selected.getResults().isEmpty())
                return;
            int imported = 0;
            juce::StringArray failures;
            for (const auto& file : selected.getResults())
            {
                if (safe->store.importFile(file, true).isNotEmpty())
                    ++imported;
                if (safe->store.getError().isNotEmpty())
                    failures.add(safe->store.getError());
            }
            safe->refresh();
            safe->status.setText(juce::String(imported) + " imported. " + failures.joinIntoString(" "),
                                 juce::dontSendNotification);
            safe->status.setTooltip(safe->status.getText());
            safe->resized();
        });
}
void PlaylistComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}
void PlaylistComponent::refresh()
{
    ids = store.list(false);
    store.sortIds(
        ids, sortColumn == durationColumn ? TrackStore::SortField::duration : TrackStore::SortField::name,
        sortAscending);
    table.updateContent();
    table.repaint();
    trackCount.setText(juce::String(ids.size()) + " tracks", juce::dontSendNotification);
    importButton.setEnabled(store.isReady());
    status.setText(store.getError(), juce::dontSendNotification);
    status.setTooltip(status.getText());
    resized();
}

juce::var PlaylistComponent::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if (rows.size() != 1 || !juce::isPositiveAndBelow(rows[0], ids.size()))
        return {};
    const auto* track = store.find(ids[rows[0]]);
    if (!track || !store.fileFor(*track).existsAsFile())
        return {};
    return "library:" + track->id;
}

void PlaylistComponent::sortOrderChanged(int columnId, bool forwards)
{
    if (columnId != titleColumn && columnId != durationColumn)
        return;
    sortColumn = columnId;
    sortAscending = forwards;
    table.deselectAllRows();
    refresh();
}
