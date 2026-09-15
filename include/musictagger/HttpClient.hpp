#pragma once

#include <string>
#include <vector>

namespace musictagger {

struct HttpTextResponse {
    long statusCode = 0;
    std::string body;
    std::string error;

    bool ok() const {
        return error.empty() && statusCode >= 200 && statusCode < 300;
    }
};

struct HttpBinaryResponse {
    long statusCode = 0;
    std::vector<unsigned char> body;
    std::string contentType;
    std::string error;

    bool ok() const {
        return error.empty() && statusCode >= 200 && statusCode < 300;
    }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    bool ready() const;
    std::string urlEncode(const std::string& value) const;

    HttpTextResponse getText(const std::string& url, const std::string& userAgent) const;
    HttpBinaryResponse getBinary(const std::string& url, const std::string& userAgent) const;

private:
    bool initialized_ = false;
};

}
