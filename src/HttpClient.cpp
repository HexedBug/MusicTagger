#include "musictagger/HttpClient.hpp"

#include <curl/curl.h>

namespace musictagger {
namespace {

size_t textWriteCallback(void* contents, size_t size, size_t nmemb, void* userData) {
    size_t totalBytes = size * nmemb;
    auto* response = static_cast<std::string*>(userData);
    response->append(static_cast<char*>(contents), totalBytes);
    return totalBytes;
}

size_t binaryWriteCallback(void* contents, size_t size, size_t nmemb, void* userData) {
    size_t totalBytes = size * nmemb;
    auto* data = static_cast<std::vector<unsigned char>*>(userData);
    auto* bytes = static_cast<unsigned char*>(contents);
    data->insert(data->end(), bytes, bytes + totalBytes);
    return totalBytes;
}

void configureCommonOptions(CURL* curl, const std::string& url, const std::string& userAgent) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
}

}

HttpClient::HttpClient() {
    initialized_ = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

HttpClient::~HttpClient() {
    if (initialized_) {
        curl_global_cleanup();
    }
}

bool HttpClient::ready() const {
    return initialized_;
}

std::string HttpClient::urlEncode(const std::string& value) const {
    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        return "";
    }

    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));

    if (encoded == nullptr) {
        curl_easy_cleanup(curl);
        return "";
    }

    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

HttpTextResponse HttpClient::getText(const std::string& url, const std::string& userAgent) const {
    HttpTextResponse response;
    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        response.error = "Could not initialize curl.";
        return response;
    }

    configureCommonOptions(curl, url, userAgent);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, textWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        response.error = curl_easy_strerror(result);
        curl_easy_cleanup(curl);
        return response;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
    curl_easy_cleanup(curl);
    return response;
}

HttpBinaryResponse HttpClient::getBinary(const std::string& url, const std::string& userAgent) const {
    HttpBinaryResponse response;
    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        response.error = "Could not initialize curl.";
        return response;
    }

    configureCommonOptions(curl, url, userAgent);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, binaryWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        response.error = curl_easy_strerror(result);
        curl_easy_cleanup(curl);
        return response;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);

    char* contentType = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);

    if (contentType != nullptr) {
        response.contentType = contentType;
    }

    curl_easy_cleanup(curl);
    return response;
}

}
