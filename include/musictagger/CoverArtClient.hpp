#pragma once

#include <optional>
#include <string>

#include "musictagger/HttpClient.hpp"
#include "musictagger/Models.hpp"

namespace musictagger {

class CoverArtClient {
public:
    explicit CoverArtClient(HttpClient& http);
    std::optional<ArtworkData> getFrontCover(const std::string& releaseId);

private:
    HttpClient& http_;
};

}
