#include "musictagger/CoverArtClient.hpp"
#include "musictagger/AppConfig.hpp"

#include <nlohmann/json.hpp>

#include <vector>

namespace musictagger {
namespace {

using json = nlohmann::json;

std::vector<std::string> findFrontCoverUrls(const json& data) {
    std::vector<std::string> urls;

    if (!data.contains("images") || !data["images"].is_array()) {
        return urls;
    }

    for (const auto& image : data["images"]) {
        if (!image.contains("front") || !image["front"].is_boolean() ||
            !image["front"].get<bool>()) {
            continue;
        }

        if (image.contains("thumbnails") && image["thumbnails"].is_object()) {
            const auto& thumbnails = image["thumbnails"];

            for (const char* size : {"1200", "500", "250"}) {
                if (thumbnails.contains(size) && thumbnails[size].is_string()) {
                    urls.push_back(thumbnails[size].get<std::string>());
                }
            }
        }

        if (image.contains("image") && image["image"].is_string()) {
            urls.push_back(image["image"].get<std::string>());
        }

        break;
    }

    return urls;
}

std::string forceHttps(std::string url) {
    if (url.rfind("http://", 0) == 0) {
        url.replace(0, 7, "https://");
    }
    return url;
}

}

CoverArtClient::CoverArtClient(HttpClient& http) : http_(http) {}

std::optional<ArtworkData> CoverArtClient::getFrontCover(const std::string& releaseId) {
    std::string metadataUrl = "https://coverartarchive.org/release/" + releaseId;
    HttpTextResponse metadataResponse = http_.getText(metadataUrl, config::kUserAgent);

    if (!metadataResponse.ok()) {
        return std::nullopt;
    }

    try {
        std::vector<std::string> urls = findFrontCoverUrls(json::parse(metadataResponse.body));

        for (std::string url : urls) {
            HttpBinaryResponse imageResponse = http_.getBinary(forceHttps(url), config::kUserAgent);

            if (!imageResponse.ok() || imageResponse.body.empty()) {
                continue;
            }

            if (!imageResponse.contentType.empty() &&
                imageResponse.contentType.rfind("image/", 0) != 0) {
                continue;
            }

            return ArtworkData{std::move(imageResponse.body), imageResponse.contentType};
        }
    } catch (const json::exception&) {
        return std::nullopt;
    }

    return std::nullopt;
}

}
