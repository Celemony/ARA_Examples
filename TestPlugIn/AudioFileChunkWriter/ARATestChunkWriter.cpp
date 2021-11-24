//------------------------------------------------------------------------------
//! \file       ARATestChunkWriter.cpp
//!             ARA audio file chunk authoring tool for the ARA test plug-in
//! \project    ARA SDK Examples
//! \copyright  Copyright (c) 2018-2021, Celemony Software GmbH, All Rights Reserved.
//! \license    Licensed under the Apache License, Version 2.0 (the "License");
//!             you may not use this file except in compliance with the License.
//!             You may obtain a copy of the License at
//!
//!               http://www.apache.org/licenses/LICENSE-2.0
//!
//!             Unless required by applicable law or agreed to in writing, software
//!             distributed under the License is distributed on an "AS IS" BASIS,
//!             WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//!             See the License for the specific language governing permissions and
//!             limitations under the License.
//------------------------------------------------------------------------------
// Command line arguments format for creating ARA audio file chunks for the ARA test plug-in:
// ./ARATestChunkWriter [-openAutomatically] [AudioFile(s)]
// The tool will add a chunk to each of the specified audio files. Existing data for other
// plug-ins will be preserved.
// If a specified file does not exist, it will be created and contain a dummy pulsed sine signal.
// openAutomatically defaults to false unless the option -openAutomatically is specified.
//------------------------------------------------------------------------------

#include <string>
#include <vector>
#include <algorithm>

#include "ARA_Library/Debug/ARADebug.h"

#include "ExamplesCommon/Archives/Archives.h"
#include "ExamplesCommon/AudioFiles/AudioFiles.h"

#include "TestAnalysis.h"
#include "TestPersistency.h"
#include "TestPlugInConfig.h"


ARA_SETUP_DEBUG_MESSAGE_PREFIX ("ARATestChunkWriter");


class SynchronousTestAnalysis : public TestAnalysisCallbacks
{
public:
    explicit SynchronousTestAnalysis (AudioFileBase& audioFile) : _audioFile { audioFile } {}

    bool readAudioSamples (int64_t samplePosition, int64_t samplesPerChannel, void* const buffers[]) noexcept override
    {
        return _audioFile.readSamples (samplePosition, samplesPerChannel, buffers, false);
    }

private:
    AudioFileBase& _audioFile;
};


void addChunk (AudioFileBase&& audioFile, bool openAutomatically)
{
    const auto persistentID { "audioSource1" };
    const auto documentArchiveID { TEST_FILECHUNK_ARCHIVE_ID };

    // perform analysis
    SynchronousTestAnalysis callbacks { audioFile };
    const auto algorithm { TestProcessingAlgorithm::getDefaultAlgorithm () };
    const auto analysisResult { algorithm->analyzeNoteContent (&callbacks, audioFile.getSampleCount (), audioFile.getSampleRate (),
                                                                static_cast<uint32_t> (audioFile.getChannelCount ())) };

    // create archive
    MemoryArchive archive { documentArchiveID };
    TestArchiver archiver {[&archive] (size_t position, size_t length, const uint8_t buffer[]) noexcept -> bool
                            {
                                return archive.writeBytes (static_cast<std::streamoff> (position), static_cast<std::streamoff> (length), reinterpret_cast<const char*> (buffer));
                            }};
    archiver.writeString (persistentID);
    archiver.writeString (algorithm->getIdentifier ());
    encodeTestNoteContent (analysisResult.get (), archiver);
    ARA_INTERNAL_ASSERT (archiver.didSucceed ());

    // store ARA audio file XML chunk
    audioFile.setiXMLARAAudioSourceData (documentArchiveID, openAutomatically,
                                         TEST_PLUGIN_NAME, TEST_VERSION_STRING, TEST_MANUFACTURER_NAME, TEST_INFORMATION_URL,
                                         persistentID, archive);

    // store audio file
    auto wavFileName { audioFile.getName () };
    auto success { audioFile.saveToFile (audioFile.getName ()) };
    ARA_INTERNAL_ASSERT (success);
}


// see start of this file for detailed description of the command line arguments
int main (int argc, const char* argv[])
{
    const std::vector<std::string> args (argv + 1, argv + argc);
    bool openAutomatically { false };
    for (const auto& it : args)
    {
        if (it == "-openAutomatically")
        {
            openAutomatically = true;
            continue;
        }

        icstdsp::AudioFile audioFile;
        int ARA_MAYBE_UNUSED_VAR (err);
        err = audioFile.Load (it.c_str ());
        if (err == icstdsp::NOFILE)
        {
            ARA_LOG ("Audio File '%s' not found, will be created.", it.c_str ());
            addChunk (SineAudioFile { it, 5.0, 44100.0, 1 }, openAutomatically);
        }
        else
        {
            ARA_INTERNAL_ASSERT (err == 0);
            addChunk (AudioDataFile { it, std::move (audioFile) }, openAutomatically);
        }
    }
    return 0;
}
