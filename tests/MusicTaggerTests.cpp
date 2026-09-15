#include <iostream>
#include <string>
#include <vector>

#include "musictagger/FilenameParser.hpp"
#include "musictagger/Models.hpp"
#include "musictagger/StringUtils.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& testName) {
    if (condition) {
        std::cout << "[PASS] " << testName << '\n';
        return;
    }

    ++failures;
    std::cerr << "[FAIL] " << testName << '\n';
}

void testStringUtils() {
    expect(musictagger::trimWhitespace("  Lithonia \t") == "Lithonia", "trimWhitespace");
    expect(musictagger::normalizeString("  Childish Gambino  ") == "childish gambino", "normalizeString");
    expect(musictagger::removeQuotes("\"C:\\Music\\song.mp3\"") == "C:\\Music\\song.mp3", "removeQuotes");
}

void testFilenameParser() {
    std::vector<musictagger::SearchCandidate> candidates = musictagger::parseFilenameCandidates(
        R"(/music/Childish Gambino - Lithonia (Audio) (1).mp3)"
    );

    expect(!candidates.empty(), "filename parser returns candidates");

    if (!candidates.empty()) {
        expect(candidates[0].artist == "Childish Gambino", "filename parser identifies artist");
        expect(candidates[0].title == "Lithonia", "filename parser removes source and duplicate suffix noise");
    }

    std::vector<musictagger::SearchCandidate> titleOnly =
        musictagger::parseFilenameCandidates(R"(/music/Lithonia.mp3)");

    expect(!titleOnly.empty(), "title-only filename returns a candidate");

    if (!titleOnly.empty()) {
        expect(titleOnly[0].artist.empty(), "title-only filename leaves artist unknown");
        expect(titleOnly[0].title == "Lithonia", "title-only filename preserves title");
    }

    std::vector<musictagger::SearchCandidate> tagged = musictagger::buildSearchCandidates(
        R"(/music/unknown.mp3)", "Childish Gambino", "Lithonia"
    );

    expect(!tagged.empty(), "embedded tags create a search candidate");

    if (!tagged.empty()) {
        expect(tagged[0].artist == "Childish Gambino", "tagged artist is preserved");
        expect(tagged[0].title == "Lithonia", "tagged title is preserved");
        expect(tagged[0].confidence == 100, "complete embedded tags receive maximum confidence");
    }
}

}

int main() {
    testStringUtils();
    testFilenameParser();

    if (failures == 0) {
        std::cout << "\nAll tests passed.\n";
        return 0;
    }

    std::cerr << "\n" << failures << " test(s) failed.\n";
    return 1;
}
