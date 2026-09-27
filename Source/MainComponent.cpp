#include "MainComponent.h"

MainComponent::MainComponent()
    : library(store,
              [this](const juce::File& file, int deck) { (deck == 0 ? deckA : deckB).loadFile(file); }),
      sampler(
          store, recorder, preview, [this] { return deviceRate.load(); },
          [this] { return enableMicrophone(); },
          [this]
          {
              const auto* device = deviceManager.getCurrentAudioDevice();
              return device != nullptr && !device->getActiveInputChannels().isZero();
          },
          [this](int deck) { return (deck == 0 ? playerA : playerB).isLoaded(); },
          [this](const juce::String& id)
          {
              deckA.unloadIf(id);
              deckB.unloadIf(id);
          })
{
    setLookAndFeel(&theme);
    formats.registerBasicFormats();
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    addMouseListener(this, true);
    audioViewport.addComponentListener(this);
    projectPanel.addComponentListener(this);
    controlsPanel.addComponentListener(this);
    for (auto* child : std::initializer_list<juce::Component*>{&deckA, &deckB, &controlsPanel,
                                                             &columnDivider, &rightColumnDivider,
                                                             &tabs, &rightTabs})
        addAndMakeVisible(child);
    controlsPanel.addAndMakeVisible(deckA.getPlaybackControls());
    controlsPanel.addAndMakeVisible(deckB.getPlaybackControls());
    projectPanel.addAndMakeVisible(projectFolder);
    projectPanel.addAndMakeVisible(status);
    columnLayout.setItemLayout(0, 280, -1.0, -0.365);
    columnLayout.setItemLayout(1, 6, 6, 6);
    columnLayout.setItemLayout(2, 320, -1.0, 320);
    columnLayout.setItemLayout(3, 6, 6, 6);
    columnLayout.setItemLayout(4, 280, -1.0, -0.365);
    columnDivider.setAlpha(0.0f);
    rightColumnDivider.setAlpha(0.0f);
    tabs.addTab("Library", theme.findColour(juce::ResizableWindow::backgroundColourId), &library, false);
    rightTabs.addTab("Sampler", theme.findColour(juce::ResizableWindow::backgroundColourId), &sampler, false);
    tabs.setTabBarDepth(30);
    rightTabs.setTabBarDepth(30);
    projectFolder.onClick = [this] { chooseProject(); };
    setSize(1200, 800);
    audioViewport.setScrollBarsShown(true, false);

    rightTabs.addTab("Audio settings", theme.findColour(juce::ResizableWindow::backgroundColourId), &audioViewport,
                false);
    tabs.addTab("Project folder", theme.findColour(juce::ResizableWindow::backgroundColourId), &projectPanel,
                false);
    resized();
    startTimerHz(5);
}
MainComponent::~MainComponent()
{
    removeKeyListener(this);
    removeMouseListener(this);
    audioViewport.removeComponentListener(this);
    projectPanel.removeComponentListener(this);
    controlsPanel.removeComponentListener(this);
    stopTimer();
    tabs.clearTabs();
    rightTabs.clearTabs();
    audioViewport.setViewedComponent(nullptr, false);
    audioPanel.reset();
    shutdownAudio();
    sampler.finishRecording();
    preview.stop();
    setLookAndFeel(nullptr);
}
void MainComponent::initialiseServices()
{
    const auto root = TrackStore::findProjectRoot();
    if (root.isDirectory())
        store.initialise(root);
    else
        startupMessage = "Choose the project folder to enable the library and recording.";

    setAudioChannels(0, 2);
    if (deviceManager.getCurrentAudioDevice() == nullptr)
        startupMessage = "Audio output unavailable. Check Audio settings.";

    if (!root.isDirectory())
        chooseProject();
}

bool MainComponent::enableMicrophone()
{
    if (const auto* device = deviceManager.getCurrentAudioDevice())
        if (!device->getActiveInputChannels().isZero())
            return true;

    // Device changes may restart audio; preserve the existing output configuration on failure.
    const auto previous = deviceManager.getAudioDeviceSetup();
    auto requested = previous;
    if (requested.inputDeviceName.isEmpty())
    {
        auto* type = deviceManager.getCurrentDeviceTypeObject();
        if (type == nullptr)
            return false;
        const auto inputs = type->getDeviceNames(true);
        if (inputs.isEmpty())
            return false;
        requested.inputDeviceName =
            inputs[juce::jlimit(0, inputs.size() - 1, type->getDefaultDeviceIndex(true))];
    }
    requested.useDefaultInputChannels = false;
    requested.inputChannels.setRange(0, 2, true);
    const auto error = deviceManager.setAudioDeviceSetup(requested, false);
    if (error.isEmpty())
        if (const auto* device = deviceManager.getCurrentAudioDevice())
            if (!device->getActiveInputChannels().isZero())
                return true;

    deviceManager.setAudioDeviceSetup(previous, false);
    return false;
}

void MainComponent::componentVisibilityChanged(juce::Component& component)
{
    if (&component != &audioViewport || !audioViewport.isVisible() || audioPanel)
        return;

    audioPanel = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 2, 2, false, false,
                                                                      true, false);
    audioViewport.setViewedComponent(audioPanel.get(), false);
    layoutSettings();
}

void MainComponent::prepareToPlay(int blockSize, double rate)
{
    playerA.prepareToPlay(blockSize, rate);
    playerB.prepareToPlay(blockSize, rate);
    const int capacity = juce::jmax(8192, blockSize);
    for (auto* buffer : {&bufferA, &bufferB, &mixBuffer, &inputBuffer})
        buffer->setSize(2, capacity);
    deviceRate = rate;
}
void MainComponent::releaseResources()
{
    recorder.requestStop();
    deviceRate = 0;
    playerA.releaseResources();
    playerB.releaseResources();
}
void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& output)
{
    if (mixBuffer.getNumSamples() == 0)
    {
        output.clearActiveBufferRegion();
        return;
    }
    int inputChannels = 0;
    if (auto* device = deviceManager.getCurrentAudioDevice())
        inputChannels = device->getActiveInputChannels().countNumberOfSetBits();
    for (int offset = 0; offset < output.numSamples;)
    {
        const int count = juce::jmin(output.numSamples - offset, mixBuffer.getNumSamples());
        inputBuffer.clear();
        for (int channel = 0; channel < 2 && inputChannels > 0; ++channel)
            inputBuffer.copyFrom(channel, 0, *output.buffer, juce::jmin(channel, inputChannels - 1),
                                 output.startSample + offset, count);
        bufferA.clear();
        bufferB.clear();
        mixBuffer.clear();
        playerA.getNextAudioBlock({&bufferA, 0, count});
        playerB.getNextAudioBlock({&bufferB, 0, count});
        for (int channel = 0; channel < 2; ++channel)
        {
            mixBuffer.copyFrom(channel, 0, bufferA, channel, 0, count);
            mixBuffer.addFrom(channel, 0, bufferB, channel, 0, count);
        }
        captureRecording(count);
        preview.addTo(mixBuffer, count, deviceRate.load());
        for (int channel = 0; channel < output.buffer->getNumChannels(); ++channel)
        {
            auto* destination = output.buffer->getWritePointer(channel, output.startSample + offset);
            if (channel < 2)
                for (int i = 0; i < count; ++i)
                    destination[i] = juce::jlimit(-1.0f, 1.0f, mixBuffer.getSample(channel, i));
            else
                juce::FloatVectorOperations::clear(destination, count);
        }
        offset += count;
    }
}
void MainComponent::captureRecording(int samples)
{
    if (!recorder.isCapturing())
        return;

    switch (recorder.getSource())
    {
    case AudioRecorder::Source::deckA:
        recorder.capture(bufferA, samples);
        break;
    case AudioRecorder::Source::deckB:
        recorder.capture(bufferB, samples);
        break;
    case AudioRecorder::Source::mix:
        recorder.capture(mixBuffer, samples);
        break;
    case AudioRecorder::Source::microphone:
        recorder.capture(inputBuffer, samples);
        break;
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}
void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto decks = area.removeFromTop((area.getHeight() - 12) * 1 / 2);
    auto leftDeck = decks.removeFromLeft((decks.getWidth() - 12) / 2);
    decks.removeFromLeft(12);
    deckA.setBounds(leftDeck);
    deckB.setBounds(decks);
    area.removeFromTop(12);
    juce::Component* columns[]{&tabs, &columnDivider, &controlsPanel, &rightColumnDivider, &rightTabs};
    columnLayout.layOutComponents(columns, 5, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                  false, true);
    layoutPlaybackControls();
    layoutSettings();
}
void MainComponent::layoutPlaybackControls()
{
    auto area = controlsPanel.getLocalBounds().reduced(10, 24);
    auto left = area.removeFromLeft((area.getWidth() - 12) / 2);
    area.removeFromLeft(12);
    const int height = left.getHeight();
    deckA.layoutPlaybackControls(left.withSizeKeepingCentre(140, height));
    deckB.layoutPlaybackControls(area.withSizeKeepingCentre(140, height));
}
void MainComponent::layoutSettings()
{
    auto settings = projectPanel.getLocalBounds().reduced(16);
    projectFolder.setBounds(settings.removeFromTop(36).removeFromLeft(190));
    settings.removeFromTop(12);
    status.setBounds(settings.removeFromTop(100));
    if (audioPanel && audioViewport.getWidth() > 0)
    {
        // JUCE computes the selector's content height.
        const int width = juce::jmax(1, audioViewport.getWidth() - audioViewport.getScrollBarThickness());
        audioPanel->setSize(width, audioPanel->getHeight());
    }
}
void MainComponent::componentMovedOrResized(juce::Component& component, bool, bool wasResized)
{
    if (wasResized)
    {
        if (&component == &controlsPanel)
            layoutPlaybackControls();
        else
            layoutSettings();
    }
}
void MainComponent::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.withOnlyMouseButtons().getRawFlags() != juce::ModifierKeys::leftButtonModifier ||
        event.mods.isPopupMenu())
        if (auto* button = dynamic_cast<juce::Button*>(event.eventComponent))
            button->setState(juce::Button::buttonNormal);
}
void MainComponent::timerCallback()
{
    if (!started)
    {
        started = true;
        initialiseServices();
    }
    status.setText(store.getError().isNotEmpty() ? store.getError()
                   : store.isReady()
                       ? "Project ready. Library, cues and samples are saved in the project data folder."
                       : startupMessage,
                   juce::dontSendNotification);
    status.setTooltip(status.getText());
    projectFolder.setEnabled(!store.isReady() && !recorder.hasRecording());
}

bool MainComponent::canTriggerKeyboardCue() const
{
    if (juce::Component::getNumCurrentlyModalComponents() > 0)
        return false;
    for (auto* focused = juce::Component::getCurrentlyFocusedComponent(); focused != nullptr;
         focused = focused->getParentComponent())
        if (dynamic_cast<juce::TextEditor*>(focused) != nullptr)
            return false;
    const auto modifiers = juce::ModifierKeys::getCurrentModifiersRealtime();
    return !modifiers.isCtrlDown() && !modifiers.isAltDown() && !modifiers.isCommandDown();
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return canTriggerKeyboardCue() && key.getKeyCode() >= '1' && key.getKeyCode() <= '8';
}

bool MainComponent::keyStateChanged(bool, juce::Component*)
{
    bool handled = false;
    const bool allowed = canTriggerKeyboardCue();
    for (size_t index = 0; index < heldCueKeys.size(); ++index)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown('1' + static_cast<int>(index));
        if (down && !heldCueKeys[index] && allowed)
        {
            auto& deck = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown() ? deckB : deckA;
            deck.triggerCue(static_cast<int>(index));
        }
        heldCueKeys[index] = down;
        handled = handled || (down && allowed);
    }
    return handled;
}
void MainComponent::chooseProject()
{
    chooser = std::make_unique<juce::FileChooser>("Choose the OtoDecks project folder");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& selected)
        {
            if (!safe || !selected.getResult().isDirectory())
                return;
            if (safe->store.initialise(selected.getResult()))
                safe->startupMessage.clear();
        });
}
