#pragma once

#include <string>
#include <vector>

#include "musictagger/Models.hpp"

namespace musictagger {

std::vector<SearchCandidate> parseFilenameCandidates(const std::string& path);

std::vector<SearchCandidate> buildSearchCandidates(
    const std::string& path,
    const std::string& taggedArtist,
    const std::string& taggedTitle
);

}
