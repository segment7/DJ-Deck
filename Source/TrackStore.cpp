/*
  ==============================================================================

    TrackStore.cpp
    Created: 7 Sep 2026 6:01:01pm
    Author:  Rey

  ==============================================================================
*/

#include "TrackStore.h"
#include <cmath>
#include <algorithm>
#include <set>

//==============================================================================
TrackStore::TrackStore() = default;

TrackStore::~TrackStore() = default;

juce::File TrackStore::findProjectRoot()
{
    const auto executableFolder =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const auto workingFolder = juce::File::getCurrentWorkingDirectory();

    for (const auto& start : {executableFolder, workingFolder})
    {
        for (auto folder = start; folder != folder.getParentDirectory(); folder = folder.getParentDirectory())
        {
            if (folder.getChildFile("OtoDecks.jucer").existsAsFile())
                return folder;
        }
    }

    return {};
}

bool TrackStore::initialise(const juce::File& projectRoot)
{
    root = projectRoot;
    ready = false;
    tracks.clear();
    error.clear();

    if (!root.isDirectory() || !samplesDirectory().createDirectory())
    {
        error = "Cannot create project data directory.";
        return false;
    }

    const auto file = stateFile();
    if (file.existsAsFile() && !loadState(file))
        return false;

    ready = true;
    sendChangeMessage();
    return true;
}

//==============================================================================
juce::String TrackStore::importFile(const juce::File& file, bool library, bool sample)
{
    error.clear();

    if (!ready)
    {
        error = "Select a valid project folder first.";
        return {};
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));

    if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
    {
        error = "Cannot read audio: " + file.getFileName();
        return {};
    }

    if (sample && !file.isAChildOf(samplesDirectory()))
    {
        error = "Samples must be inside the project samples folder.";
        return {};
    }

    const auto duration = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;

    for (auto& track : tracks)
    {
        if (fileFor(track) != file)
            continue;

        track.inLibrary = track.inLibrary || library;
        track.duration = duration;

        for (auto& cue : track.cues)
        {
            if (cue > duration)
                cue = -1;
        }

        const auto id = track.id;
        saveAndNotify();
        return id;
    }

    Track track;
    track.id = juce::Uuid().toString();
    track.path = file.isAChildOf(root) ? file.getRelativePathFrom(root) : file.getFullPathName();
    track.name = file.getFileName();
    track.duration = duration;
    track.inLibrary = library;
    track.sample = sample;

    const auto id = track.id;
    tracks.push_back(std::move(track));
    saveAndNotify();
    return id;
}

const TrackStore::Track* TrackStore::find(const juce::String& id) const
{
    for (const auto& track : tracks)
    {
        if (track.id == id)
            return &track;
    }

    return nullptr;
}

void TrackStore::sortIds(juce::StringArray& ids, SortField field, bool ascending) const
{
    std::sort(ids.begin(), ids.end(),
              [this, field, ascending](const juce::String& first, const juce::String& second)
              {
                  const auto* a = find(first);
                  const auto* b = find(second);
                  if ((a == nullptr) != (b == nullptr))
                      return a != nullptr;

                  int comparison = 0;
                  if (a && b)
                  {
                      if (field == SortField::name)
                          comparison = a->name.compareIgnoreCase(b->name);
                      else if (a->duration < b->duration)
                          comparison = -1;
                      else if (a->duration > b->duration)
                          comparison = 1;
                  }
                  if (comparison == 0)
                      comparison = first.compare(second);
                  return ascending ? comparison < 0 : comparison > 0;
              });
}
juce::StringArray TrackStore::list(bool samples) const
{
    juce::StringArray result;

    for (const auto& track : tracks)
    {
        if (samples ? track.sample : track.inLibrary)
            result.add(track.id);
    }

    return result;
}

juce::File TrackStore::fileFor(const Track& track) const
{
    return root.getChildFile(track.path);
}

bool TrackStore::remove(const juce::String& id)
{
    error.clear();

    for (auto iterator = tracks.begin(); iterator != tracks.end(); ++iterator)
    {
        if (iterator->id != id)
            continue;

        if (iterator->sample)
        {
            const auto file = fileFor(*iterator);
            if (!file.isAChildOf(samplesDirectory()) || (file.exists() && !file.deleteFile()))
            {
                error = "Cannot delete sample file.";
                return false;
            }

            tracks.erase(iterator);
        }
        else
        {
            // Keep identity and cues when removing a library listing.
            iterator->inLibrary = false;
        }

        saveAndNotify();
        return error.isEmpty();
    }

    return false;
}

//==============================================================================
bool TrackStore::setCue(const juce::String& id, int index, double seconds)
{
    if (index < 0 || index >= 8)
        return false;

    for (auto& track : tracks)
    {
        if (track.id != id || !isValidCuePosition(seconds, track.duration))
            continue;

        track.cues[static_cast<size_t>(index)] = seconds;
        saveAndNotify();
        return true;
    }

    return false;
}

void TrackStore::clearCues(const juce::String& id)
{
    for (auto& track : tracks)
    {
        if (track.id != id)
            continue;

        track.cues.fill(-1);
        saveAndNotify();
        return;
    }
}

//==============================================================================
bool TrackStore::save()
{
    if (!ready)
        return false;

    juce::Array<juce::var> rows;
    for (const auto& track : tracks)
        rows.add(serialiseTrack(track));

    // The var owns the reference-counted DynamicObject.
    juce::var state(new juce::DynamicObject());
    state.getDynamicObject()->setProperty("version", 1);
    state.getDynamicObject()->setProperty("tracks", rows);

    juce::TemporaryFile temporary(stateFile());
    if (!temporary.getFile().replaceWithText(juce::JSON::toString(state)) ||
        !temporary.overwriteTargetFileWithTemporary())
    {
        error = "Cannot save state; changes remain in memory. Check folder permissions.";
        return false;
    }

    error.clear();
    return true;
}

juce::File TrackStore::samplesDirectory() const
{
    return root.getChildFile("data/samples");
}

juce::String TrackStore::getError() const
{
    return error;
}

bool TrackStore::isReady() const
{
    return ready;
}

//==============================================================================
bool TrackStore::loadState(const juce::File& file)
{
    juce::var state;
    const auto result = juce::JSON::parse(file.loadFileAsString(), state);

    // Keep the owning var alive while accessing its array.
    const auto rowsValue = state.getProperty("tracks", {});
    const auto* rows = rowsValue.getArray();

    if (result.failed() || static_cast<int>(state.getProperty("version", 0)) != 1 || rows == nullptr)
    {
        error = "Invalid state.json. Original preserved; choose another project folder or repair the file.";
        return false;
    }

    std::vector<Track> loadedTracks;
    std::set<juce::String> ids;
    std::set<juce::String> paths;
    loadedTracks.reserve(static_cast<size_t>(rows->size()));

    for (const auto& row : *rows)
    {
        Track track;
        if (!parseTrack(row, track))
        {
            error = "Invalid track data. Original state.json preserved.";
            return false;
        }

        const auto audioFile = fileFor(track);
        if (!ids.insert(track.id).second || !paths.insert(audioFile.getFullPathName()).second ||
            (track.sample && !audioFile.isAChildOf(samplesDirectory())))
        {
            error = "Invalid track data. Original state.json preserved.";
            return false;
        }

        loadedTracks.push_back(std::move(track));
    }

    tracks = std::move(loadedTracks);
    return true;
}

bool TrackStore::parseTrack(const juce::var& value, Track& output)
{
    output.id = value.getProperty("id", {}).toString();
    output.path = value.getProperty("path", {}).toString();
    output.name = value.getProperty("name", {}).toString();
    output.duration = static_cast<double>(value.getProperty("duration", 0.0));
    output.inLibrary = static_cast<bool>(value.getProperty("library", false));
    output.sample = static_cast<bool>(value.getProperty("sample", false));

    const auto cuesValue = value.getProperty("cues", {});
    const auto* cues = cuesValue.getArray();

    if (output.id.isEmpty() || output.path.isEmpty() || !std::isfinite(output.duration) ||
        output.duration <= 0 || cues == nullptr || cues->size() != static_cast<int>(output.cues.size()))
        return false;

    for (int index = 0; index < cues->size(); ++index)
    {
        const auto& cue = cues->getReference(index);
        if (!(cue.isDouble() || cue.isInt() || cue.isInt64()))
            return false;

        const auto seconds = static_cast<double>(cue);
        if (!isValidCuePosition(seconds, output.duration))
            return false;

        output.cues[static_cast<size_t>(index)] = seconds;
    }

    return true;
}

juce::var TrackStore::serialiseTrack(const Track& track)
{
    juce::var row(new juce::DynamicObject());
    auto* object = row.getDynamicObject();

    object->setProperty("id", track.id);
    object->setProperty("path", track.path);
    object->setProperty("name", track.name);
    object->setProperty("duration", track.duration);
    object->setProperty("library", track.inLibrary);
    object->setProperty("sample", track.sample);

    juce::Array<juce::var> cues;
    for (const auto cue : track.cues)
        cues.add(cue);

    object->setProperty("cues", cues);
    return row;
}

bool TrackStore::isValidCuePosition(double seconds, double duration)
{
    return std::isfinite(seconds) && (seconds == -1 || (seconds >= 0 && seconds <= duration));
}

juce::File TrackStore::stateFile() const
{
    return root.getChildFile("data/state.json");
}

void TrackStore::saveAndNotify()
{
    save();
    sendChangeMessage();
}
