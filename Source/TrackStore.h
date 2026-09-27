/*
  ==============================================================================

    TrackStore.h
    Created: 7 Sep 2026 6:01:01pm
    Author:  Rey

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

//==============================================================================
/** Owns track metadata and saved cues; access this store on the message thread. */
class TrackStore : public juce::ChangeBroadcaster
{
public:
    /** A copyable metadata record; callers receive read-only access to stored records. */
    struct Track
    {
        juce::String id;
        juce::String path;
        juce::String name;
        double duration = 0.0;
        bool inLibrary = false;
        bool sample = false;
        std::array<double, 8> cues{-1, -1, -1, -1, -1, -1, -1, -1};
    };

    //==============================================================================
    /** Creates an empty store; call initialise before importing files. */
    TrackStore();

    /** Releases stored records and pending change notifications. */
    ~TrackStore() override;

    /** Returns the project folder above the executable or working directory, or an empty File. */
    static juce::File findProjectRoot();

    /** Loads project data; returns false and preserves the state file if loading fails. */
    bool initialise(const juce::File& projectRoot);

    //==============================================================================
    /** Imports file metadata; library lists it, sample marks an owned recording. Returns an ID or empty. */
    juce::String importFile(const juce::File& file, bool library, bool sample = false);

    /** Returns a read-only record or nullptr; do not retain the pointer across mutations. */
    const Track* find(const juce::String& id) const;

    /** Selects metadata used to order track IDs. */
    enum class SortField
    {
        name,
        duration
    };

    /** Sorts IDs by case-insensitive name or seconds, then ID, in the requested direction.*/
    void sortIds(juce::StringArray& ids, SortField field, bool ascending) const;

    /** Returns sample IDs when true, or music library IDs when false. */
    juce::StringArray list(bool samples) const;

    /** Resolves a record's absolute or project-relative path. */
    juce::File fileFor(const Track& track) const;

    /** Removes a library entry or owned sample file; returns false on removal or save failure. */
    bool remove(const juce::String& id);

    //==============================================================================
    /** Sets cue 0-7 to valid track seconds, or -1 to clear; reports the in-memory update result. */
    bool setCue(const juce::String& id, int index, double seconds);

    /** Clears all cues for the ID; an unknown ID leaves the store unchanged. */
    void clearCues(const juce::String& id);

    //==============================================================================
    /** Saves via a temporary file; returns false if unavailable or writing fails. */
    bool save();

    /** Returns the project's directory for recorded samples. */
    juce::File samplesDirectory() const;

    /** Returns the latest storage error; check after mutations for save failures. */
    juce::String getError() const;

    /** Returns whether project data was successfully initialised. */
    bool isReady() const;

private:
    //==============================================================================
    /** Loads and validates all records before replacing the in-memory collection. */
    bool loadState(const juce::File& file);

    /** Decodes one record into output; returns false for invalid metadata or cues. */
    static bool parseTrack(const juce::var& value, Track& output);

    /** Returns a JSON-compatible object containing one track's saved fields. */
    static juce::var serialiseTrack(const Track& track);

    /** Accepts a cleared cue (-1) or finite seconds within the track duration. */
    static bool isValidCuePosition(double seconds, double duration);

    /** Returns the project's state file. */
    juce::File stateFile() const;

    /** Attempts a save and asynchronously notifies views of the in-memory change. */
    void saveAndNotify();

    std::vector<Track> tracks;
    juce::File root;
    juce::String error;
    bool ready = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackStore)
};
