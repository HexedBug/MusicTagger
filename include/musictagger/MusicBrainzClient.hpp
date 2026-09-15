#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "musictagger/HttpClient.hpp"
#include "musictagger/Models.hpp"

namespace musictagger {

class MusicBrainzClient {
public:
    explicit MusicBrainzClient(HttpClient& http);

    RecordingLookupResult identifyRecording(const std::vector<SearchCandidate>& candidates);
    std::optional<std::vector<ReleaseCandidate>> getReleases(
        const std::vector<RecordingMatch>& recordings);
    std::optional<TaggingDetails> getTaggingDetails(const std::string& releaseId,
                                                    const std::string& recordingId);

private:
    HttpTextResponse get(const std::string& url);
    void waitForRateLimit();

    HttpClient& http_;
    std::chrono::steady_clock::time_point lastRequest_;
};

}
