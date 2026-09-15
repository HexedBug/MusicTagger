#include "musictagger/Application.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "musictagger/CoverArtClient.hpp"
#include "musictagger/FilenameParser.hpp"
#include "musictagger/HttpClient.hpp"
#include "musictagger/Models.hpp"
#include "musictagger/MusicBrainzClient.hpp"
#include "musictagger/StringUtils.hpp"
#include "musictagger/TagWriter.hpp"

namespace musictagger {
namespace {

void displayCurrentTags(const ExistingTags& tags) {
    std::cout << "\n--- Current Tags ---\n";
    std::cout << "Title:  " << tags.title << '\n';
    std::cout << "Artist: " << tags.artist << '\n';
    std::cout << "Album:  " << tags.album << '\n';
    std::cout << "Year:   " << tags.year << '\n';
    std::cout << "Track:  " << tags.track << '\n';
}

std::string promptRequiredValue(const std::string& prompt) {
    while (true) {
        std::cout << prompt;

        std::string value;
        std::getline(std::cin, value);
        value = trimWhitespace(value);

        if (!value.empty()) {
            return value;
        }

        std::cout << "Please enter a value.\n";
    }
}

std::string getInputPath(int argc, char* argv[]) {
    if (argc > 1) {
        return removeQuotes(argv[1]);
    }

    std::cout << "Enter path to a music file: ";

    std::string path;
    std::getline(std::cin, path);
    return removeQuotes(path);
}

bool hasKnownArtist(const std::vector<SearchCandidate>& candidates) {
    return std::any_of(candidates.begin(), candidates.end(), [](const SearchCandidate& candidate) {
        return !candidate.artist.empty();
    });
}

bool hasKnownTitle(const std::vector<SearchCandidate>& candidates) {
    return std::any_of(candidates.begin(), candidates.end(), [](const SearchCandidate& candidate) {
        return !candidate.title.empty();
    });
}

void addManualSearchFallback(std::vector<SearchCandidate>& candidates, const ExistingTags& tags) {
    bool artistKnown = hasKnownArtist(candidates);
    bool titleKnown = hasKnownTitle(candidates);

    if (artistKnown && titleKnown) {
        return;
    }

    std::string manualArtist;
    std::string manualTitle;

    if (!titleKnown) {
        std::cout << "\nCould not determine the song title from the tags or filename.\n";
        manualTitle = promptRequiredValue("Enter song title: ");
    }

    if (!artistKnown) {
        std::string detectedTitle = !tags.title.empty() ? tags.title : manualTitle;

        if (detectedTitle.empty()) {
            auto titleIt = std::find_if(candidates.begin(), candidates.end(), [](const SearchCandidate& candidate) {
                return !candidate.title.empty();
            });

            if (titleIt != candidates.end()) {
                detectedTitle = titleIt->title;
            }
        }

        std::cout << "\nCould not determine the artist from the tags or filename.\n";

        if (!detectedTitle.empty()) {
            std::cout << "Detected title: " << detectedTitle << '\n';
        }

        manualArtist = promptRequiredValue("Enter artist name: ");
    }

    std::vector<SearchCandidate> manualCandidates;

    if (!manualArtist.empty() && !manualTitle.empty()) {
        manualCandidates.push_back({manualArtist, manualTitle, "manual input", 100});
    } else if (!manualArtist.empty()) {
        for (const SearchCandidate& candidate : candidates) {
            if (candidate.title.empty()) {
                continue;
            }

            manualCandidates.push_back({
                manualArtist,
                candidate.title,
                "manual artist + " + candidate.source,
                std::min(100, candidate.confidence + 20)
            });
        }

        if (manualCandidates.empty() && !tags.title.empty()) {
            manualCandidates.push_back({manualArtist, tags.title, "manual artist + tagged title", 100});
        }
    } else if (!manualTitle.empty()) {
        for (const SearchCandidate& candidate : candidates) {
            if (candidate.artist.empty()) {
                continue;
            }

            manualCandidates.push_back({
                candidate.artist,
                manualTitle,
                "manual title + " + candidate.source,
                std::min(100, candidate.confidence + 20)
            });
        }

        if (manualCandidates.empty() && !tags.artist.empty()) {
            manualCandidates.push_back({tags.artist, manualTitle, "tagged artist + manual title", 100});
        }
    }

    candidates.insert(candidates.end(), manualCandidates.begin(), manualCandidates.end());

    std::sort(candidates.begin(), candidates.end(), [](const SearchCandidate& a, const SearchCandidate& b) {
        return a.confidence > b.confidence;
    });
}

void displaySearchCandidates(const std::vector<SearchCandidate>& candidates) {
    std::cout << "\n--- Search Candidates ---\n";

    for (size_t i = 0; i < candidates.size(); ++i) {
        const SearchCandidate& candidate = candidates[i];

        std::cout << "\n[" << i + 1 << "]\n";
        std::cout << "Artist: " << (candidate.artist.empty() ? "(unknown)" : candidate.artist) << '\n';
        std::cout << "Title: " << candidate.title << '\n';
        std::cout << "Source: " << candidate.source << '\n';
        std::cout << "Parser confidence: " << candidate.confidence << '\n';
    }
}

void displayRecordingMatches(const std::vector<RecordingMatch>& recordings) {
    const RecordingMatch& best = recordings.front();

    std::cout << "\n--- Best Match ---\n";
    std::cout << "Title: " << best.title << '\n';
    std::cout << "Artist: " << best.artist << '\n';
    std::cout << "MusicBrainz score: " << best.score << '\n';
    std::cout << "Recording ID: " << best.id << '\n';
    std::cout << "Matched using: " << best.matchedCandidate.source << '\n';

    if (recordings.size() > 1) {
        std::cout << "Exact MusicBrainz recording entries found: " << recordings.size() << '\n';
        std::cout << "Release options will be collected from all exact matches.\n";
    }
}

void displayReleases(const std::vector<ReleaseCandidate>& releases) {
    std::cout << "\n--- Release Candidates ---\n";

    for (size_t i = 0; i < releases.size(); ++i) {
        const ReleaseCandidate& release = releases[i];

        std::cout << "\n[" << i + 1 << "]\n";
        std::cout << "Release: " << release.title << '\n';

        if (!release.type.empty()) {
            std::cout << "Type: " << release.type << '\n';
        }
        if (!release.date.empty()) {
            std::cout << "Date: " << release.date << '\n';
        }
        if (!release.country.empty()) {
            std::cout << "Country: " << release.country << '\n';
        }
        if (release.trackCount > 0) {
            std::cout << "Tracks: " << release.trackCount << '\n';
        }

        std::cout << "Release ID: " << release.releaseId << '\n';
    }
}

int chooseRelease(const std::vector<ReleaseCandidate>& releases) {
    while (true) {
        std::cout << "\nChoose release (1-" << releases.size() << "): ";

        std::string input;
        std::getline(std::cin, input);

        try {
            size_t processed = 0;
            int choice = std::stoi(input, &processed);

            if (processed == input.size() && choice >= 1 && choice <= static_cast<int>(releases.size())) {
                return choice - 1;
            }
        } catch (...) {
        }

        std::cout << "Invalid selection.\n";
    }
}

void displayTagPreview(const RecordingMatch& recording,
                       const ReleaseCandidate& release,
                       const TaggingDetails& details,
                       bool hasArtwork) {
    std::cout << "\n--- Tags To Write ---\n";
    std::cout << "Title:        " << recording.title << '\n';
    std::cout << "Artist:       " << recording.artist << '\n';
    std::cout << "Album:        " << release.title << '\n';
    std::cout << "Album Artist: " << details.albumArtist << '\n';
    std::cout << "Date:         " << release.date << '\n';
    std::cout << "Country:      " << release.country << '\n';
    std::cout << "Release Type: " << release.type << '\n';

    if (details.trackNumber > 0) {
        std::cout << "Track:        " << details.trackNumber;
        if (details.trackTotal > 0) {
            std::cout << "/" << details.trackTotal;
        }
        std::cout << '\n';
    }

    if (details.discNumber > 0) {
        std::cout << "Disc:         " << details.discNumber;
        if (details.discTotal > 1) {
            std::cout << "/" << details.discTotal;
        }
        std::cout << '\n';
    }

    std::cout << "Artwork:      " << (hasArtwork ? "Yes" : "No") << '\n';
}

bool confirmWrite() {
    std::cout << "\nWrite these tags to the file? (y/n): ";

    std::string response;
    std::getline(std::cin, response);
    response = normalizeString(response);

    return response == "y" || response == "yes";
}

}

int runApplication(int argc, char* argv[]) {
    HttpClient http;

    if (!http.ready()) {
        std::cerr << "Could not initialize HTTP services.\n";
        return 1;
    }

    std::string path = getInputPath(argc, argv);

    if (path.empty()) {
        std::cerr << "No file path was provided.\n";
        return 1;
    }

    std::optional<ExistingTags> existingTags = tagging::readTags(path);

    if (!existingTags) {
        std::cerr << "Could not open the music file.\n";
        return 1;
    }

    displayCurrentTags(*existingTags);

    std::vector<SearchCandidate> candidates =
        buildSearchCandidates(path, existingTags->artist, existingTags->title);

    addManualSearchFallback(candidates, *existingTags);

    if (candidates.empty()) {
        std::cerr << "Could not determine useful search information.\n";
        return 1;
    }

    displaySearchCandidates(candidates);

    MusicBrainzClient musicBrainz(http);

    std::cout << "\nSearching MusicBrainz...\n";
    RecordingLookupResult lookup = musicBrainz.identifyRecording(candidates);

    if (lookup.status == LookupStatus::ServiceUnavailable) {
        std::cerr << "MusicBrainz is currently unavailable. Try again later.\n";
        return 1;
    }

    if (lookup.status == LookupStatus::NoMatch) {
        std::cerr << "Could not confidently identify the recording.\n";
        return 1;
    }

    if (!lookup.ok()) {
        std::cerr << "MusicBrainz error: " << lookup.message << '\n';
        return 1;
    }

    displayRecordingMatches(lookup.recordings);

    std::optional<std::vector<ReleaseCandidate>> releases = musicBrainz.getReleases(lookup.recordings);

    if (!releases || releases->empty()) {
        std::cerr << "Could not retrieve release information.\n";
        return 1;
    }

    displayReleases(*releases);

    int selectedIndex = chooseRelease(*releases);
    const ReleaseCandidate& selectedRelease = (*releases)[selectedIndex];

    auto recordingIt = std::find_if(
        lookup.recordings.begin(), lookup.recordings.end(), [&selectedRelease](const RecordingMatch& recording) {
            return recording.id == selectedRelease.recordingId;
        }
    );

    if (recordingIt == lookup.recordings.end()) {
        std::cerr << "Could not determine the recording for the selected release.\n";
        return 1;
    }

    const RecordingMatch& selectedRecording = *recordingIt;

    std::optional<TaggingDetails> details =
        musicBrainz.getTaggingDetails(selectedRelease.releaseId, selectedRelease.recordingId);

    if (!details) {
        std::cerr << "Could not retrieve track details for the selected release.\n";
        return 1;
    }

    CoverArtClient coverArt(http);

    std::cout << "\nLooking for cover artwork...\n";
    std::optional<ArtworkData> artwork = coverArt.getFrontCover(selectedRelease.releaseId);

    if (artwork) {
        std::cout << "Front artwork downloaded (" << artwork->bytes.size() << " bytes).\n";
    } else {
        std::cout << "No front artwork was available. Continuing without it.\n";
    }

    displayTagPreview(selectedRecording, selectedRelease, *details, artwork.has_value());

    if (!confirmWrite()) {
        std::cout << "No changes were made.\n";
        return 0;
    }

    const ArtworkData* artworkPointer = artwork ? &*artwork : nullptr;

    if (!tagging::writeTags(path, selectedRecording, selectedRelease, *details, artworkPointer)) {
        std::cerr << "Could not write tags to the file.\n";
        return 1;
    }

    std::cout << "\nFile tagged successfully.\n";
    return 0;
}

}
