#include "Application.h"
#include "MainComponent.h"

OtoDecksApplication::OtoDecksApplication() = default;
const juce::String OtoDecksApplication::getApplicationName()
{
    return ProjectInfo::projectName;
}
const juce::String OtoDecksApplication::getApplicationVersion()
{
    return ProjectInfo::versionString;
}
bool OtoDecksApplication::moreThanOneInstanceAllowed()
{
    return false;
}
void OtoDecksApplication::initialise(const juce::String&)
{
    mainWindow = std::make_unique<MainWindow>(getApplicationName());
}
void OtoDecksApplication::shutdown()
{
    mainWindow.reset();
}
void OtoDecksApplication::systemRequestedQuit()
{
    quit();
}
void OtoDecksApplication::anotherInstanceStarted(const juce::String&)
{
    if (mainWindow)
        mainWindow->toFront(true);
}
OtoDecksApplication::MainWindow::MainWindow(const juce::String& name)
    : DocumentWindow(name, juce::Colour(0xff12161c), DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new MainComponent(), true);
    setResizable(true, true);
    setResizeLimits(1000, 700, 2400, 1600);
    centreWithSize(1200, 800);
    setVisible(true);
}
void OtoDecksApplication::MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

START_JUCE_APPLICATION(OtoDecksApplication)
