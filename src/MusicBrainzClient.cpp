#include "musictagger/MusicBrainzClient.hpp"
#include "musictagger/AppConfig.hpp"
#include "musictagger/StringUtils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <thread>
#include <unordered_set>
#include <utility>

namespace musictagger {
namespace {

using json = nlohmann::json;

constexpr auto kRequestDelay = std::chrono::milliseconds(1500);
constexpr int kMaxAttempts = 3;

std::string getArtistCredit(const json& data) {
    if (!data.contains("artist-credit") || !data["artist-credit"].is_array()) {
        return "";
    }

    std::string result;

    for (const auto& credit : data["artist-credit"]) {
        if (credit.contains("name") && credit["name"].is_string()) {
            result += credit["name"].get<std::string>();
        } else if (credit.contains("artist") && credit["artist"].is_object() &&
                   credit["artist"].contains("name") && credit["artist"]["name"].is_string()) {
            result += credit["artist"]["name"].get<std::string>();
        }

        if (credit.contains("joinphrase") && credit["joinphrase"].is_string()) {
            result += credit["joinphrase"].get<std::string>();
        }
    }

    return result;
}

std::vector<RecordingMatch> findExactRecordings(const json& data, const SearchCandidate& search) {
    std::vector<RecordingMatch> matches;
    std::unordered_set<std::string> seenIds;

    if (!data.contains("recordings") || !data["recordings"].is_array()) {
        return matches;
    }

    for (const auto& recording : data["recordings"]) {
        if (!recording.contains("id") || !recording["id"].is_string() ||
            !recording.contains("title") || !recording["title"].is_string()) {
            continue;
        }

        std::string recordingId = recording["id"].get<std::string>();
        std::string resultTitle = recording["title"].get<std::string>();
        std::string resultArtist = getArtistCredit(recording);

        if (resultArtist.empty()) {
            continue;
        }

        bool titleMatches = normalizeString(resultTitle) == normalizeString(search.title);
        bool artistMatches = normalizeString(resultArtist) == normalizeString(search.artist);

        if (!artistMatches && recording.contains("artist-credit") &&
            recording["artist-credit"].is_array() && !recording["artist-credit"].empty()) {
            const auto& firstArtist = recording["artist-credit"][0];

            if (firstArtist.contains("name") && firstArtist["name"].is_string()) {
                artistMatches = normalizeString(firstArtist["name"].get<std::string>()) ==
                                normalizeString(search.artist);
            }
        }

        if (!titleMatches || !artistMatches || seenIds.find(recordingId) != seenIds.end()) {
            continue;
        }

        int score = 0;
        if (recording.contains("score") && recording["score"].is_number_integer()) {
            score = recording["score"].get<int>();
        }

        matches.push_back({recordingId, resultTitle, resultArtist, score, search});
        seenIds.insert(recordingId);
    }

    std::sort(matches.begin(), matches.end(), [](const RecordingMatch& a, const RecordingMatch& b) {
        return a.score > b.score;
    });

    return matches;
}

std::vector<ReleaseCandidate> extractReleases(const json& recordingData,
                                              const std::string& recordingId) {
    std::vector<ReleaseCandidate> releases;

    if (!recordingData.contains("releases") || !recordingData["releases"].is_array()) {
        return releases;
    }

    for (const auto& release : recordingData["releases"]) {
        ReleaseCandidate candidate;
        candidate.recordingId = recordingId;

        if (release.contains("id") && release["id"].is_string()) {
            candidate.releaseId = release["id"].get<std::string>();
        }

        if (release.contains("title") && release["title"].is_string()) {
            candidate.title = release["title"].get<std::string>();
        }

        if (release.contains("date") && release["date"].is_string()) {
            candidate.date = release["date"].get<std::string>();
        }

        if (release.contains("country") && release["country"].is_string()) {
            candidate.country = release["country"].get<std::string>();
        }

        if (release.contains("release-group") && release["release-group"].is_object()) {
            const auto& group = release["release-group"];
            if (group.contains("primary-type") && group["primary-type"].is_string()) {
                candidate.type = group["primary-type"].get<std::string>();
            }
        }

        if (release.contains("media") && release["media"].is_array()) {
            for (const auto& medium : release["media"]) {
                if (medium.contains("track-count") && medium["track-count"].is_number_integer()) {
                    candidate.trackCount += medium["track-count"].get<int>();
                }
            }
        }

        if (!candidate.releaseId.empty()) {
            releases.push_back(candidate);
        }
    }

    return releases;
}

TaggingDetails extractTaggingDetails(const json& releaseData, const std::string& recordingId) {
    TaggingDetails details;
    details.albumArtist = getArtistCredit(releaseData);

    if (!releaseData.contains("media") || !releaseData["media"].is_array()) {
        return details;
    }

    details.discTotal = static_cast<int>(releaseData["media"].size());

    for (const auto& medium : releaseData["media"]) {
        int discPosition = 0;
        int trackCount = 0;

        if (medium.contains("position") && medium["position"].is_number_integer()) {
            discPosition = medium["position"].get<int>();
        }

        if (medium.contains("track-count") && medium["track-count"].is_number_integer()) {
            trackCount = medium["track-count"].get<int>();
        }

        if (!medium.contains("tracks") || !medium["tracks"].is_array()) {
            continue;
        }

        for (const auto& track : medium["tracks"]) {
            if (!track.contains("recording") || !track["recording"].is_object() ||
                !track["recording"].contains("id") || !track["recording"]["id"].is_string()) {
                continue;
            }

            if (track["recording"]["id"].get<std::string>() != recordingId) {
                continue;
            }

            details.discNumber = discPosition;
            details.trackTotal = trackCount;

            if (track.contains("position") && track["position"].is_number_integer()) {
                details.trackNumber = track["position"].get<int>();
            }

            if (track.contains("id") && track["id"].is_string()) {
                details.releaseTrackId = track["id"].get<std::string>();
            }

            return details;
        }
    }

    return details;
}

}

MusicBrainzClient::MusicBrainzClient(HttpClient& http)
    : http_(http),
      lastRequest_(std::chrono::steady_clock::now() - std::chrono::seconds(5)) {}

void MusicBrainzClient::waitForRateLimit() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRequest_);

    if (elapsed < kRequestDelay) {
        std::this_thread::sleep_for(kRequestDelay - elapsed);
    }
}

HttpTextResponse MusicBrainzClient::get(const std::string& url) {
    HttpTextResponse response;

    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        waitForRateLimit();
        response = http_.getText(url, config::kUserAgent);
        lastRequest_ = std::chrono::steady_clock::now();

        if (response.statusCode != 429 && response.statusCode != 503) {
            return response;
        }

        if (attempt < kMaxAttempts) {
            std::this_thread::sleep_for(std::chrono::seconds(attempt * 2));
        }
    }

    return response;
}

RecordingLookupResult MusicBrainzClient::identifyRecording(
    const std::vector<SearchCandidate>& candidates) {
    for (const SearchCandidate& candidate : candidates) {
        if (candidate.artist.empty()) {
            continue;
        }

        std::string query = "recording:\"" + candidate.title + "\" AND artistname:\"" +
                            candidate.artist + "\"";
        std::string encodedQuery = http_.urlEncode(query);

        if (encodedQuery.empty()) {
            return {LookupStatus::Error, {}, "Could not encode MusicBrainz query."};
        }

        std::string url = "https://musicbrainz.org/ws/2/recording?query=" + encodedQuery +
                          "&fmt=json&limit=25";

        HttpTextResponse response = get(url);

        if (response.statusCode == 429 || response.statusCode == 503) {
            return {LookupStatus::ServiceUnavailable, {},
                    "MusicBrainz is temporarily unavailable."};
        }

        if (!response.ok()) {
            std::string message = response.error.empty()
                ? "MusicBrainz returned HTTP " + std::to_string(response.statusCode) + "."
                : response.error;
            return {LookupStatus::Error, {}, message};
        }

        try {
            json data = json::parse(response.body);
            std::vector<RecordingMatch> matches = findExactRecordings(data, candidate);

            if (!matches.empty()) {
                return {LookupStatus::Success, std::move(matches), ""};
            }
        } catch (const json::exception& e) {
            return {LookupStatus::Error, {}, e.what()};
        }
    }

    return {LookupStatus::NoMatch, {}, "No exact MusicBrainz match was found."};
}

std::optional<std::vector<ReleaseCandidate>> MusicBrainzClient::getReleases(
    const std::vector<RecordingMatch>& recordings) {
    std::vector<ReleaseCandidate> releases;
    std::unordered_set<std::string> seenReleaseIds;

    for (const RecordingMatch& recording : recordings) {
        std::string url = "https://musicbrainz.org/ws/2/recording/" + recording.id +
                          "?inc=releases+release-groups+media&fmt=json";

        HttpTextResponse response = get(url);

        if (!response.ok()) {
            return std::nullopt;
        }

        try {
            std::vector<ReleaseCandidate> recordingReleases =
                extractReleases(json::parse(response.body), recording.id);

            for (ReleaseCandidate& release : recordingReleases) {
                if (seenReleaseIds.insert(release.releaseId).second) {
                    releases.push_back(std::move(release));
                }
            }
        } catch (const json::exception&) {
            return std::nullopt;
        }
    }

    return releases;
}

std::optional<TaggingDetails> MusicBrainzClient::getTaggingDetails(
    const std::string& releaseId, const std::string& recordingId) {
    std::string url = "https://musicbrainz.org/ws/2/release/" + releaseId +
                      "?inc=recordings+artist-credits+media&fmt=json";

    HttpTextResponse response = get(url);

    if (!response.ok()) {
        return std::nullopt;
    }

    try {
        return extractTaggingDetails(json::parse(response.body), recordingId);
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

}
