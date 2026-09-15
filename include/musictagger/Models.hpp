#pragma once

#include <string>
#include <vector>

namespace musictagger {

struct SearchCandidate {
    std::string artist;
    std::string title;
    std::string source;
    int confidence = 0;
};

struct ReleaseCandidate {
    std::string releaseId;
    std::string recordingId;
    std::string title;
    std::string date;
    std::string country;
    std::string type;
    int trackCount = 0;
};

struct ArtworkData {
    std::vector<unsigned char> bytes;
    std::string contentType;
};

struct TaggingDetails {
    std::string albumArtist;
    std::string releaseTrackId;
    int trackNumber = 0;
    int trackTotal = 0;
    int discNumber = 0;
    int discTotal = 0;
};

struct ExistingTags {
    std::string title;
    std::string artist;
    std::string album;
    unsigned int year = 0;
    unsigned int track = 0;
};

struct RecordingMatch {
    std::string id;
    std::string title;
    std::string artist;
    int score = 0;
    SearchCandidate matchedCandidate;
};

enum class LookupStatus {
    Success,
    NoMatch,
    ServiceUnavailable,
    Error
};

struct RecordingLookupResult {
    LookupStatus status = LookupStatus::Error;
    std::vector<RecordingMatch> recordings;
    std::string message;

    bool ok() const {
        return status == LookupStatus::Success && !recordings.empty();
    }
};

}
