#pragma once
#include <JuceHeader.h>

class OtoDecksApplication : public juce::JUCEApplication
{
public:
    /** Creates the application instance. */
    OtoDecksApplication();
    /** Returns the configured product name. */
    const juce::String getApplicationName() override;
    /** Returns the configured product version. */
    const juce::String getApplicationVersion() override;
    /** Disallows concurrent writers to the project state. */
    bool moreThanOneInstanceAllowed() override;
    /** Opens the main window; command-line arguments are unused. */
    void initialise(const juce::String& commandLine) override;
    /** Releases the window and its audio components. */
    void shutdown() override;
    /** Requests a graceful application exit. */
    void systemRequestedQuit() override;
    /** Brings the existing window forward on a second launch. */
    void anotherInstanceStarted(const juce::String& commandLine) override;

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        /** Creates a resizable application window and owns its content. */
        explicit MainWindow(const juce::String& name);
        /** Requests application shutdown. */
        void closeButtonPressed() override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
    std::unique_ptr<MainWindow> mainWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OtoDecksApplication)
};
