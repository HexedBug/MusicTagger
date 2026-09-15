#pragma once

#include <optional>
#include <string>

#include "musictagger/Models.hpp"

namespace musictagger::tagging {

std::optional<ExistingTags> readTags(const std::string& path);

bool writeTags(const std::string& path,
               const RecordingMatch& recording,
               const ReleaseCandidate& release,
               const TaggingDetails& details,
               const ArtworkData* artwork = nullptr);

}
