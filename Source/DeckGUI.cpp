// Extended from the template by Matthew.
#include "DeckGUI.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
juce::String clockText(double seconds)
{
    const auto ticks = static_cast<juce::int64>(std::llround(juce::jmax(0.0, seconds) * 100.0));
    return juce::String(ticks / 6000).paddedLeft('0', 2) + ":" +
           juce::String((ticks / 100) % 60).paddedLeft('0', 2) + "." +
           juce::String(ticks % 100).paddedLeft('0', 2);
}
} // namespace
DeckGUI::DeckGUI(DJAudioPlayer& audio, TrackStore& data, juce::AudioFormatManager& formats,
                 juce::AudioThumbnailCache& cache, juce::String name, juce::Colour colour)
    : player(audio), store(data), accent(colour), waveform(formats, cache)
{
    setName(name);
    initialiseTransport();
    initialiseLabels();
    initialiseSliders();
    waveform.setSeekCallback(
        [this](double value)
        {
            player.setPositionRelative(value);
            refresh();
        });
    stop.setTooltip("Stop and return to the beginning");
    stop.setTitle("Stop");
    volume.setTitle("Volume");
    speed.setTitle("Playback speed");
    for (size_t i = 0; i < cueButtons.size(); ++i)
    {
        auto& button = cueButtons[i];
        button.setButtonText(juce::String((int)i + 1));
        button.addListener(this);
        button.addMouseListener(this, false);
        addAndMakeVisible(button);
    }
    addAndMakeVisible(waveform);
    for (auto* control : std::initializer_list<juce::Component*>{&volumeLabel, &speedLabel, &volume, &speed,
                                                               &play, &stop})
        levelControls.addAndMakeVisible(control);
    for (size_t index = 0; index < sliderMargins.size(); ++index)
        (index < 2 ? volume : speed).addAndMakeVisible(sliderMargins[index]);
    player.setGain(volume.getValue() / 100.0);
    player.setSpeed(speed.getValue());
    refresh();
    startTimerHz(25);
}
void DeckGUI::initialiseTransport()
{
    playIcon = juce::Drawable::createFromImageData(BinaryData::Fluent_Emoji_high_contrast_25b6_svg,
                                                   BinaryData::Fluent_Emoji_high_contrast_25b6_svgSize);
    pauseIcon = juce::Drawable::createFromImageData(BinaryData::Fluent_Emoji_high_contrast_23f8_svg,
                                                    BinaryData::Fluent_Emoji_high_contrast_23f8_svgSize);
    stopIcon = juce::Drawable::createFromImageData(BinaryData::Fluent_Emoji_high_contrast_23f9_svg,
                                                   BinaryData::Fluent_Emoji_high_contrast_23f9_svgSize);
    jassert(playIcon && pauseIcon && stopIcon);
    for (auto* icon : {playIcon.get(), pauseIcon.get(), stopIcon.get()})
        if (icon)
            icon->replaceColour(juce::Colour(0xff212121), juce::Colour(0xffedf1f6));
    play.setImages(playIcon.get());
    stop.setImages(stopIcon.get());
    play.setEdgeIndent(5);
    stop.setEdgeIndent(5);
}

void DeckGUI::initialiseLabels()
{
    trackName.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    volumeLabel.setText("Volume", juce::dontSendNotification);
    speedLabel.setText("Speed", juce::dontSendNotification);
    for (auto* label : {&volumeLabel, &speedLabel})
        label->setJustificationType(juce::Justification::centred);
    time.setEditable(true, true, true);
    time.setTooltip("Click to enter mm:ss.xx or seconds; Enter seeks, Escape cancels.");
    time.onEditorShow = [this] { editingTrackId = trackId; };
    time.onTextChange = [this] { applyEditedPosition(); };
    cueHint.setText(getName() == "DECK A" ? "DECK A Hotkey : 1-8 | Right-click to edit"
                                          : "DECK B Hotkey : Shift + 1-8 | Right-click to edit",
                    juce::dontSendNotification);
    cueHint.setFont(juce::FontOptions(12.0f));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffffc285));
    for (auto* label : {&trackName, &time, &duration, &volumeLabel, &speedLabel, &cueHint, &status})
        addAndMakeVisible(label);
    for (auto* button : std::initializer_list<juce::Button*>{&play, &stop, &load, &eject, &clear})
    {
        addAndMakeVisible(button);
        button->addListener(this);
    }
}

void DeckGUI::initialiseSliders()
{
    for (auto* slider : {&volume, &speed})
    {
        addAndMakeVisible(slider);
        slider->setSliderStyle(juce::Slider::LinearVertical);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 24);
        slider->setScrollWheelEnabled(false);
        slider->setWantsKeyboardFocus(false);
        slider->setColour(juce::Slider::trackColourId, accent);
        slider->setColour(juce::Slider::thumbColourId, accent);
        slider->addListener(this);
    }
    volume.setRange(0, 100, 1);
    volume.setValue(100);
    volume.setTextValueSuffix(" %");
    volume.setDoubleClickReturnValue(true, 100);
    speed.setRange(0.25, 2, 0.01);
    speed.setValue(1);
    speed.setTextValueSuffix(" x");
    speed.setDoubleClickReturnValue(true, 1.0);
}

DeckGUI::~DeckGUI()
{
    stopTimer();
    for (auto& button : cueButtons)
        button.removeMouseListener(this);
}
void DeckGUI::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);
}
void DeckGUI::resized()
{
    auto area = getLocalBounds().reduced(16, 8);
    status.setBounds(area.removeFromTop(16));
    auto heading = area.removeFromTop(58);
    trackName.setBounds(heading.removeFromTop(30));
    auto fileActions = heading.removeFromRight(140);
    eject.setBounds(fileActions.removeFromRight(68).reduced(2));
    load.setBounds(fileActions.removeFromRight(68).reduced(2));
    time.setBounds(heading.removeFromLeft(100));
    duration.setBounds(heading);
    auto pads = area.removeFromBottom(30);
    clear.setBounds(pads.removeFromRight(92).reduced(2));
    const int width = pads.getWidth() / 8;
    for (auto& button : cueButtons)
        button.setBounds(pads.removeFromLeft(width).reduced(2));
    cueHint.setBounds(area.removeFromBottom(18));
    waveform.setBounds(area);
}

void DeckGUI::layoutPlaybackControls(juce::Rectangle<int> bounds)
{
    levelControls.setBounds(bounds);
    auto levels = levelControls.getLocalBounds();
    auto transport = levels.removeFromTop(54);
    transport.removeFromRight(15);
    stop.setBounds(transport.removeFromRight(56));
    transport.removeFromRight(6);
    play.setBounds(transport.removeFromRight(56));
    levels.removeFromTop(12);
    auto speedArea = levels.removeFromLeft(levels.getWidth() / 2);
    auto volumeArea = levels;
    speedArea.removeFromBottom(12);
    volumeArea.removeFromBottom(12);
    speedLabel.setBounds(speedArea.removeFromBottom(22));
    volumeLabel.setBounds(volumeArea.removeFromBottom(22));
    speed.setBounds(speedArea.withSizeKeepingCentre(64, speedArea.getHeight()));
    volume.setBounds(volumeArea.withSizeKeepingCentre(64, volumeArea.getHeight()));
    for (size_t index = 0; index < sliderMargins.size(); ++index)
    {
        const auto& slider = index < 2 ? volume : speed;
        sliderMargins[index].setBounds(index % 2 == 0 ? 0 : 40, 0, 24,
                                       juce::jmax(0, slider.getHeight() - 24));
    }
}
void DeckGUI::buttonClicked(juce::Button* button)
{
    if (button == &play)
    {
        if (player.isPlaying())
            player.pause();
        else
            player.start();
    }
    else if (button == &stop)
        player.stop();
    else if (button == &eject)
        unload();
    else if (button == &clear)
        store.clearCues(trackId);
    else if (button == &load)
    {
        chooser = std::make_unique<juce::FileChooser>("Load one audio track", juce::File(),
                                                      "*.wav;*.mp3;*.aiff;*.flac;*.ogg");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<DeckGUI>(this)](const juce::FileChooser& selected)
            {
                if (safe && selected.getResult().existsAsFile())
                    safe->loadFile(selected.getResult());
            });
    }
    else
        for (size_t i = 0; i < cueButtons.size(); ++i)
            if (button == &cueButtons[i])
                triggerCue(static_cast<int>(i));
    refresh();
}
void DeckGUI::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volume)
        player.setGain(volume.getValue() / 100.0);
    else if (slider == &speed)
        player.setSpeed(speed.getValue());
}
bool DeckGUI::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1;
}
void DeckGUI::filesDropped(const juce::StringArray& files, int, int)
{
    if (files.size() == 1)
        loadFile(juce::File(files[0]));
}
void DeckGUI::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
        for (size_t i = 0; i < cueButtons.size(); ++i)
            if (event.eventComponent == &cueButtons[i])
                editCue((int)i);
}
void DeckGUI::editCue(int index)
{
    if (!player.isLoaded())
        return;
    juce::PopupMenu menu;
    menu.addItem(1, "Update to current position");
    menu.addItem(2, "Delete cue");
    const auto id = trackId;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&cueButtons[(size_t)index]),
                       [safe = juce::Component::SafePointer<DeckGUI>(this), id, index](int result)
                       {
                           if (!safe || safe->trackId != id)
                               return;
                           if (result == 1)
                               safe->store.setCue(id, index, safe->player.getPositionSeconds());
                           else if (result == 2)
                               safe->store.setCue(id, index, -1);
                           safe->refresh();
                       });
}
bool DeckGUI::loadFile(const juce::File& file)
{
    const auto id = store.importFile(file, false);
    if (id.isEmpty() || !player.loadFile(file))
    {
        status.setText(store.getError().isNotEmpty() ? store.getError() : "Unable to load track.",
                       juce::dontSendNotification);
        return false;
    }
    trackId = id;
    waveform.loadFile(file);
    status.setText(store.getError(), juce::dontSendNotification);
    refresh();
    return true;
}
void DeckGUI::unload()
{
    player.unload();
    trackId.clear();
    waveform.loadFile({});
    status.setText({}, juce::dontSendNotification);
    refresh();
}
void DeckGUI::unloadIf(const juce::String& id)
{
    if (trackId == id)
        unload();
}
void DeckGUI::timerCallback()
{
    refresh();
}
void DeckGUI::refresh()
{
    const bool loaded = player.isLoaded();
    for (auto* button : std::initializer_list<juce::Button*>{&play, &stop, &eject, &clear})
        button->setEnabled(loaded);
    load.setEnabled(store.isReady());
    if (showingPause != player.isPlaying())
    {
        showingPause = player.isPlaying();
        play.setImages(showingPause ? pauseIcon.get() : playIcon.get());
    }
    play.setTooltip(player.isPlaying() ? "Pause" : "Play");
    play.setTitle(player.isPlaying() ? "Pause" : "Play");
    time.setEditable(loaded, loaded, true);
    if (!time.isBeingEdited())
        time.setText(clockText(player.getPositionSeconds()), juce::dontSendNotification);
    duration.setText("/ " + clockText(player.getDurationSeconds()), juce::dontSendNotification);
    std::array<double, 8> marks{-1, -1, -1, -1, -1, -1, -1, -1};
    const auto* track = store.find(trackId);
    trackName.setText(track ? track->name : "No track loaded", juce::dontSendNotification);
    status.setTooltip(status.getText());
    if (track)
        marks = track->cues;
    for (size_t i = 0; i < cueButtons.size(); ++i)
    {
        cueButtons[i].setEnabled(loaded);
        if (marks[i] >= 0)
            cueButtons[i].setColour(juce::TextButton::buttonColourId, accent.darker(0.5f));
        else
            cueButtons[i].removeColour(juce::TextButton::buttonColourId);
        cueButtons[i].setButtonText(juce::String((int)i + 1) + (marks[i] >= 0 ? " *" : ""));
        cueButtons[i].setTooltip(marks[i] >= 0 ? clockText(marks[i]) + " - right-click to edit"
                                               : "Set cue at current position");
    }
    waveform.update(player.getPositionRelative(), marks, accent);
}
void DeckGUI::triggerCue(int index)
{
    if (!player.isLoaded() || !juce::isPositiveAndBelow(index, 8))
        return;
    if (const auto* track = store.find(trackId))
    {
        const auto cue = track->cues[static_cast<size_t>(index)];
        if (cue < 0)
            store.setCue(trackId, index, player.getPositionSeconds());
        else
            player.setPosition(cue);
        refresh();
    }
}

bool DeckGUI::isInterestedInDragSource(const SourceDetails& details)
{
    const auto description = details.description.toString();
    const bool libraryDrag = description.startsWith("library:");
    const bool sampleDrag = description.startsWith("sample:");
    if (!libraryDrag && !sampleDrag)
        return false;
    const auto* track = store.find(description.substring(libraryDrag ? 8 : 7));
    return track && (libraryDrag ? track->inLibrary : track->sample) && store.fileFor(*track).existsAsFile();
}

void DeckGUI::itemDropped(const SourceDetails& details)
{
    if (!isInterestedInDragSource(details))
        return;
    const auto description = details.description.toString();
    if (const auto* track = store.find(description.substring(description.startsWith("library:") ? 8 : 7)))
        loadFile(store.fileFor(*track));
}

void DeckGUI::applyEditedPosition()
{
    const auto parts = juce::StringArray::fromTokens(time.getText().trim(), ":", "");
    auto isNumber = [](const juce::String& value)
    {
        return value.isNotEmpty() && value.containsOnly("0123456789.") && value.containsAnyOf("0123456789") &&
               value.indexOfChar('.') == value.lastIndexOfChar('.');
    };
    bool valid = player.isLoaded() && editingTrackId == trackId && (parts.size() == 1 || parts.size() == 2);
    double seconds = 0.0;
    if (valid)
    {
        valid = isNumber(parts[parts.size() - 1]);
        seconds = parts[parts.size() - 1].getDoubleValue();
        if (parts.size() == 2)
        {
            valid = valid && parts[0].isNotEmpty() && parts[0].containsOnly("0123456789") && seconds < 60.0;
            seconds += parts[0].getDoubleValue() * 60.0;
        }
        valid = valid && std::isfinite(seconds) && seconds >= 0 && seconds <= player.getDurationSeconds();
    }
    if (valid)
    {
        player.setPosition(seconds);
        status.setText({}, juce::dontSendNotification);
    }
    else
        status.setText("Enter a valid track position: mm:ss.xx or seconds.", juce::dontSendNotification);
    refresh();
}
