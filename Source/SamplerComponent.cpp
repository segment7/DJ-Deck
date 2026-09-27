/*
  ==============================================================================

    SamplerComponent.cpp
    Created: 8 Sep 2026 6:20:03pm
    Author:  Rey

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SamplerComponent.h"
#include <BinaryData.h>
#include <cmath>

//==============================================================================
SamplerComponent::SamplerComponent(TrackStore& data, AudioRecorder& capture, LoopPreview& playback,
                                   std::function<double()> rate, std::function<bool()> enableInput,
                                   std::function<bool()> inputAvailable, std::function<bool(int)> loaded,
                                   std::function<void(const juce::String&)> eject)
    : store(data), recorder(capture), preview(playback), sampleRate(std::move(rate)),
      enableMicrophone(std::move(enableInput)), microphoneAvailable(std::move(inputAvailable)),
      deckLoaded(std::move(loaded)), unload(std::move(eject))
{
    initialiseControls();
    store.addChangeListener(this);
    refreshSampleList();
    startTimerHz(10);
}

SamplerComponent::~SamplerComponent()
{
    stopTimer();
    store.removeChangeListener(this);
    finishRecording();
    preview.stop();

    for (auto* button : {&record, &finish})
        button->removeListener(this);

    table.setModel(nullptr);
}

void SamplerComponent::initialiseControls()
{
    playIcon = juce::Drawable::createFromImageData(BinaryData::Fluent_Emoji_high_contrast_25b6_svg,
                                                   BinaryData::Fluent_Emoji_high_contrast_25b6_svgSize);
    stopIcon = juce::Drawable::createFromImageData(BinaryData::Fluent_Emoji_high_contrast_23f9_svg,
                                                   BinaryData::Fluent_Emoji_high_contrast_23f9_svgSize);
    for (auto* icon : {playIcon.get(), stopIcon.get()})
        if (icon)
            icon->replaceColour(juce::Colour(0xff212121), juce::Colour(0xffedf1f6));
    sourceLabel.setText("Record from", juce::dontSendNotification);
    source.addItemList({"Deck A", "Deck B", "Deck A + B", "Microphone"}, 1);
    source.setSelectedId(1, juce::dontSendNotification);
    source.setComponentID("sampler.source");
    source.setTooltip("Choose a recording source.");
    source.onChange = [this]
    {
        if (recorder.hasRecording())
            return;
        if (source.getSelectedId() == 4)
            showStatus(enableMicrophone() ? "Microphone ready."
                                          : "Microphone unavailable. Select an input in Audio settings.");
        else
            showStatus("Ready to record " + source.getText() + ".");
        updateControls();
    };

    record.setComponentID("sampler.record");
    finish.setComponentID("sampler.finish");
    status.setComponentID("sampler.status");

    for (auto* control :
         std::initializer_list<juce::Component*>{&sourceLabel, &source, &status, &table, &emptyList})
        addAndMakeVisible(control);

    for (auto* button : {&record, &finish})
    {
        addAndMakeVisible(button);
        button->addListener(this);
    }

    emptyList.setText("No samples yet. Record from a deck or microphone.", juce::dontSendNotification);
    emptyList.setJustificationType(juce::Justification::centred);
    emptyList.setColour(juce::Label::textColourId, juce::Colour(0xffaeb8c6));
    emptyList.setInterceptsMouseClicks(false, false);

    table.setModel(this);
    table.setRowHeight(32);
    table.getHeader().addColumn("Sample", sampleColumn, 220, 100);
    table.getHeader().addColumn("Time", durationColumn, 74);
    table.getHeader().addColumn("Loop", previewColumn, 56, 30, -1, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn("File", deleteColumn, 92, 30, -1, juce::TableHeaderComponent::notSortable);
    table.getHeader().setSortColumnId(sampleColumn, true);
    showStatus("Choose a source, then Record.");
}

void SamplerComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    graphics.setColour(juce::Colours::grey);
    graphics.drawRect(getLocalBounds(), 1);
}

void SamplerComponent::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto toolbar = area.removeFromTop(38);
    sourceLabel.setBounds(toolbar.removeFromLeft(90));
    source.setBounds(toolbar.removeFromLeft(150).reduced(0, 4));

    if (area.getWidth() < 650)
        toolbar = area.removeFromTop(38);
    else
        toolbar.removeFromLeft(12);

    for (auto* button : {&record, &finish})
        button->setBounds(toolbar.removeFromLeft(126).reduced(4));

    status.setBounds(area.removeFromTop(32));
    table.setBounds(area);
    table.getHeader().setColumnWidth(sampleColumn, juce::jmax(100, table.getWidth() - 240));
    emptyList.setBounds(area.withTrimmedTop(table.getHeader().getHeight()).reduced(12));
}

//==============================================================================
int SamplerComponent::getNumRows()
{
    return ids.size();
}

const TrackStore::Track* SamplerComponent::trackForRow(int row) const
{
    return juce::isPositiveAndBelow(row, ids.size()) ? store.find(ids[row]) : nullptr;
}

void SamplerComponent::paintRowBackground(juce::Graphics& graphics, int row, int, int, bool selected)
{
    juce::ignoreUnused(row);
    graphics.fillAll(selected ? juce::Colours::lightsalmon : juce::Colours::transparentWhite);
}

void SamplerComponent::paintCell(juce::Graphics& graphics, int row, int column, int width, int height,
                                 bool selected)
{
    const auto* track = trackForRow(row);
    if (track == nullptr)
        return;

    const bool exists = store.fileFor(*track).existsAsFile();
    graphics.setColour(selected ? juce::Colours::black
                       : exists ? juce::Colours::white
                                : juce::Colours::lightsalmon);

    juce::String text;
    if (column == sampleColumn)
        text = (exists ? "" : "[Missing] ") + track->name;
    else if (column == durationColumn)
        text = juce::String(track->duration, 2) + " s";

    graphics.drawText(text, 10, 0, width - 20, height, juce::Justification::centredLeft);
}

juce::String SamplerComponent::getCellTooltip(int rowNumber, int columnId)
{
    if (columnId != sampleColumn && columnId != durationColumn)
        return {};

    if (const auto* track = trackForRow(rowNumber))
        return track->name;

    return {};
}

juce::Component* SamplerComponent::refreshComponentForCell(int row, int column, bool,
                                                           juce::Component* existing)
{
    const auto* track = trackForRow(row);
    if (track == nullptr || (column != previewColumn && column != deleteColumn))
    {
        delete existing;
        return nullptr;
    }

    const bool deleting = column == deleteColumn;

    auto* button = dynamic_cast<juce::Button*>(existing);
    if ((deleting && dynamic_cast<juce::TextButton*>(existing) == nullptr) ||
        (!deleting && dynamic_cast<juce::DrawableButton*>(existing) == nullptr))
    {
        delete existing;
        button = deleting ? static_cast<juce::Button*>(new juce::TextButton())
                          : new juce::DrawableButton("Play loop", juce::DrawableButton::ImageFitted);
    }

    const auto id = track->id;
    if (deleting)
        button->setButtonText("Delete WAV");
    if (auto* drawable = dynamic_cast<juce::DrawableButton*>(button))
    {
        drawable->setEdgeIndent(5);
        drawable->setImages(id == previewId ? stopIcon.get() : playIcon.get());
    }
    button->setEnabled(deleting || id == previewId || store.fileFor(*track).existsAsFile());

    if (deleting)
        button->setTooltip({});
    else
        button->setTooltip(id == previewId ? "Stop loop" : "Play loop");

    // Rebind even reused row components, and defer updates until this button callback returns.
    button->onClick = [safe = juce::Component::SafePointer<SamplerComponent>(this), id, deleting]
    {
        juce::MessageManager::callAsync(
            [safe, id, deleting]
            {
                if (safe == nullptr)
                    return;

                if (deleting)
                    safe->deleteSample(id);
                else
                    safe->previewSample(id);
            });
    };

    return button;
}

//==============================================================================
void SamplerComponent::buttonClicked(juce::Button* button)
{
    if (button == &record)
        startRecording();
    else if (button == &finish)
        finishRecording();
}

void SamplerComponent::startRecording()
{
    if (recorder.hasRecording())
        return;

    if (!store.isReady())
    {
        showStatus("Choose a valid project folder before recording.");
        return;
    }

    const int selected = source.getSelectedId();
    if (selected < 1 || selected > 4)
    {
        showStatus("Choose a recording source.");
        return;
    }

    if (selected == 4 && !microphoneAvailable())
    {
        showStatus("Microphone unavailable. Select an input in Audio settings.");
        return;
    }

    if ((selected == 1 && !deckLoaded(0)) || (selected == 2 && !deckLoaded(1)) ||
        (selected == 3 && !deckLoaded(0) && !deckLoaded(1)))
    {
        showStatus("Load and play a track on the selected deck before recording.");
        return;
    }

    const auto nextFile = store.samplesDirectory().getChildFile("Sample-" + juce::Uuid().toString() + ".wav");
    const auto error =
        recorder.start(nextFile, sampleRate(), static_cast<AudioRecorder::Source>(selected - 1));

    if (error.isNotEmpty())
    {
        showStatus(error);
    }
    else
    {
        recordingFile = nextFile;
        recordingSourceName = source.getText();
        showStatus("Recording " + recordingSourceName + "...");
    }

    updateControls();
}

void SamplerComponent::finishRecording()
{
    if (!recorder.hasRecording())
        return;

    if (recorder.finish())
    {
        const auto id = store.importFile(recordingFile, false, true);
        showStatus(id.isNotEmpty() && store.getError().isEmpty() ? "Saved WAV. Select Loop to preview."
                                                                 : store.getError());
    }
    else
    {
        auto message = recorder.getError();
        if (recordingFile.existsAsFile() && recordingFile.isAChildOf(store.samplesDirectory()) &&
            !recordingFile.deleteFile())
            message += " Could not remove the incomplete WAV.";

        showStatus(message);
    }

    recordingFile = {};
    recordingSourceName.clear();
    refreshSampleList();
}

void SamplerComponent::previewSample(const juce::String& id)
{
    if (previewId == id)
    {
        stopSamplePreview();
        return;
    }
    const auto* track = store.find(id);
    if (track == nullptr || !track->sample)
        return;

    if (preview.load(store.fileFor(*track)))
    {
        previewId = id;
        showStatus("Looping " + track->name);
    }
    else
    {
        showStatus("Cannot preview this sample. Check that its file exists and is at most 30 seconds.");
    }

    refreshSampleList();
}

void SamplerComponent::stopSamplePreview()
{
    preview.stop();
    previewId.clear();
    showStatus("Preview stopped.");
    refreshSampleList();
}

void SamplerComponent::deleteSample(const juce::String& id)
{
    const auto* track = store.find(id);
    if (track == nullptr || !track->sample)
        return;

    if (previewId == id)
    {
        preview.stop();
        previewId.clear();
    }

    unload(id);
    const bool removed = store.remove(id);
    showStatus(removed ? "Sample WAV deleted." : store.getError());
    refreshSampleList();
}

//==============================================================================
void SamplerComponent::timerCallback()
{
    if (recorder.hasRecording() && !recorder.isCapturing())
        finishRecording();
    else if (recorder.isCapturing())
        showStatus("Recording " + recordingSourceName + "   " + juce::String(recorder.getSeconds(), 1) +
                   " / 30.0 s");

    updateControls();
}

void SamplerComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshSampleList();
}

void SamplerComponent::refreshSampleList()
{
    ids = store.list(true);
    store.sortIds(
        ids, sortColumn == durationColumn ? TrackStore::SortField::duration : TrackStore::SortField::name,
        sortAscending);
    emptyList.setVisible(ids.isEmpty());
    table.updateContent();
    table.repaint();
    updateControls();
}

void SamplerComponent::updateControls()
{
    const bool pending = recorder.hasRecording();
    const auto rate = sampleRate();
    const int selected = source.getSelectedId();

    record.setEnabled(store.isReady() && !pending && std::isfinite(rate) && rate > 0.0 && selected >= 1 &&
                      selected <= 4 && (selected != 4 || microphoneAvailable()));
    finish.setEnabled(pending);
    source.setEnabled(!pending);
}

void SamplerComponent::showStatus(const juce::String& message)
{
    status.setText(message, juce::dontSendNotification);
}

void SamplerComponent::sortOrderChanged(int columnId, bool forwards)
{
    if (columnId != sampleColumn && columnId != durationColumn)
        return;
    sortColumn = columnId;
    sortAscending = forwards;
    table.deselectAllRows();
    refreshSampleList();
}

juce::var SamplerComponent::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if (rows.size() != 1)
        return {};
    const auto* sample = trackForRow(rows[0]);
    if (!sample || !store.fileFor(*sample).existsAsFile())
        return {};
    return "sample:" + sample->id;
}
