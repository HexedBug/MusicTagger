#include "musictagger/FilenameParser.hpp"
#include "musictagger/StringUtils.hpp"

#include <algorithm>
#include <filesystem>
#include <regex>

namespace musictagger {
namespace {

std::string cleanSourceTitle(std::string title) {
    static const std::regex bracketNoise(
        R"(\s*[\(\[][^)\]]*(official\s+(music\s+)?video|official\s+video|official\s+audio|official\s+lyric\s+video|lyric\s+video|with\s+lyrics|visualizer|audio|4k|hd\s+remaster)[^)\]]*[\)\]]\s*)",
        std::regex_constants::icase
    );

    static const std::regex lyricsSuffix(R"(\s*//\s*lyrics\s*$)", std::regex_constants::icase);
    static const std::regex extraSpaces(R"(\s{2,})");

    title = std::regex_replace(title, bracketNoise, " ");
    title = std::regex_replace(title, lyricsSuffix, "");
    title = std::regex_replace(title, extraSpaces, " ");

    return trimWhitespace(title);
}

std::string removeDuplicateFileSuffix(std::string title) {
    static const std::regex duplicateSuffix(R"(\s*\(\d+\)\s*$)");
    return trimWhitespace(std::regex_replace(title, duplicateSuffix, ""));
}

std::string removeFeatureCredit(std::string title) {
    static const std::regex bracketFeature(
        R"(\s*[\(\[]\s*(feat\.?|ft\.?|featuring)\s+[^)\]]+[\)\]]\s*$)",
        std::regex_constants::icase
    );

    static const std::regex plainFeature(
        R"(\s+(feat\.?|ft\.?|featuring)\s+.+$)",
        std::regex_constants::icase
    );

    title = std::regex_replace(title, bracketFeature, "");
    title = std::regex_replace(title, plainFeature, "");

    return trimWhitespace(title);
}

std::string removeProductionCredit(std::string title) {
    static const std::regex productionCredit(
        R"(\s*[\(\[]\s*prod\.?\s+[^)\]]+[\)\]]\s*$)",
        std::regex_constants::icase
    );

    title = std::regex_replace(title, productionCredit, "");
    return trimWhitespace(title);
}

void addSearchCandidate(std::vector<SearchCandidate>& candidates,
                        const std::string& artist,
                        const std::string& title,
                        const std::string& source,
                        int confidence) {
    std::string cleanArtist = trimWhitespace(artist);
    std::string cleanTitle = trimWhitespace(title);

    if (cleanTitle.empty()) {
        return;
    }

    confidence = std::clamp(confidence, 0, 100);

    for (SearchCandidate& existing : candidates) {
        bool sameArtist = normalizeString(existing.artist) == normalizeString(cleanArtist);
        bool sameTitle = normalizeString(existing.title) == normalizeString(cleanTitle);

        if (sameArtist && sameTitle) {
            if (confidence > existing.confidence) {
                existing.confidence = confidence;
                existing.source = source;
            }
            return;
        }
    }

    candidates.push_back({cleanArtist, cleanTitle, source, confidence});
}

size_t findLooseHyphen(const std::string& text) {
    int parenthesisDepth = 0;
    int bracketDepth = 0;
    size_t foundPosition = std::string::npos;
    int foundCount = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];

        if (ch == '(') {
            ++parenthesisDepth;
        } else if (ch == ')' && parenthesisDepth > 0) {
            --parenthesisDepth;
        } else if (ch == '[') {
            ++bracketDepth;
        } else if (ch == ']' && bracketDepth > 0) {
            --bracketDepth;
        } else if (ch == '-' && parenthesisDepth == 0 && bracketDepth == 0) {
            foundPosition = i;
            ++foundCount;
        }
    }

    return foundCount == 1 ? foundPosition : std::string::npos;
}

}

std::vector<SearchCandidate> parseFilenameCandidates(const std::string& path) {
    std::vector<SearchCandidate> guesses;
    std::filesystem::path filePath(path);

    std::string filename = cleanSourceTitle(filePath.stem().string());
    filename = removeDuplicateFileSuffix(filename);

    const std::vector<std::string> separators = {" - ", " – ", " — "};

    size_t bestPosition = std::string::npos;
    size_t bestLength = 0;

    for (const std::string& separator : separators) {
        size_t position = filename.find(separator);

        if (position != std::string::npos &&
            (bestPosition == std::string::npos || position < bestPosition)) {
            bestPosition = position;
            bestLength = separator.length();
        }
    }

    int forwardConfidence = 75;
    int reverseConfidence = 60;

    if (bestPosition == std::string::npos) {
        size_t looseHyphen = findLooseHyphen(filename);

        if (looseHyphen != std::string::npos) {
            bestPosition = looseHyphen;
            bestLength = 1;
            forwardConfidence = 55;
            reverseConfidence = 50;
        }
    }

    if (bestPosition == std::string::npos) {
        addSearchCandidate(guesses, "", filename, "filename title only", 45);
        return guesses;
    }

    std::string left = trimWhitespace(filename.substr(0, bestPosition));
    std::string right = trimWhitespace(filename.substr(bestPosition + bestLength));

    if (left.empty() || right.empty()) {
        addSearchCandidate(guesses, "", filename, "filename title only", 40);
        return guesses;
    }

    addSearchCandidate(guesses, left, right, "filename: artist - title", forwardConfidence);
    addSearchCandidate(guesses, right, left, "filename: title - artist", reverseConfidence);

    return guesses;
}

std::vector<SearchCandidate> buildSearchCandidates(const std::string& path,
                                                   const std::string& taggedArtist,
                                                   const std::string& taggedTitle) {
    std::vector<SearchCandidate> candidates;

    if (!taggedArtist.empty() && !taggedTitle.empty()) {
        addSearchCandidate(candidates, taggedArtist, taggedTitle, "embedded tags", 100);
    }

    std::vector<SearchCandidate> filenameGuesses = parseFilenameCandidates(path);

    for (const SearchCandidate& guess : filenameGuesses) {
        int confidence = guess.confidence;

        if (!taggedArtist.empty() && !guess.artist.empty() &&
            normalizeString(taggedArtist) == normalizeString(guess.artist)) {
            confidence += 20;
        }

        if (!taggedTitle.empty() && normalizeString(taggedTitle) == normalizeString(guess.title)) {
            confidence += 20;
        }

        addSearchCandidate(candidates, guess.artist, guess.title, guess.source, confidence);

        if (taggedArtist.empty() && !guess.artist.empty() && !taggedTitle.empty()) {
            addSearchCandidate(candidates, guess.artist, taggedTitle,
                               "filename artist + tagged title", 95);
        }

        if (!taggedArtist.empty() && !guess.title.empty()) {
            addSearchCandidate(candidates, taggedArtist, guess.title,
                               "tagged artist + filename title", 95);
        }
    }

    if (!taggedTitle.empty()) {
        addSearchCandidate(candidates, taggedArtist, taggedTitle,
                           "tagged title", taggedArtist.empty() ? 65 : 100);
    }

    std::vector<SearchCandidate> originalCandidates = candidates;

    for (const SearchCandidate& candidate : originalCandidates) {
        std::string withoutFeature = removeFeatureCredit(candidate.title);

        if (normalizeString(withoutFeature) != normalizeString(candidate.title)) {
            addSearchCandidate(candidates, candidate.artist, withoutFeature,
                               candidate.source + " - feature removed",
                               candidate.confidence - 5);
        }

        std::string withoutProduction = removeProductionCredit(candidate.title);

        if (normalizeString(withoutProduction) != normalizeString(candidate.title)) {
            addSearchCandidate(candidates, candidate.artist, withoutProduction,
                               candidate.source + " - production credit removed",
                               candidate.confidence - 5);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const SearchCandidate& a, const SearchCandidate& b) {
                  return a.confidence > b.confidence;
              });

    return candidates;
}

}
