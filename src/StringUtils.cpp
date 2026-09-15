#include "musictagger/StringUtils.hpp"

#include <algorithm>
#include <cctype>

namespace musictagger {

std::string trimWhitespace(std::string text) {
    text.erase(
        text.begin(),
        std::find_if(text.begin(), text.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        })
    );

    text.erase(
        std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(),
        text.end()
    );

    return text;
}

std::string normalizeString(std::string text) {
    text = trimWhitespace(text);

    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return text;
}

std::string removeQuotes(std::string text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }

    return text;
}

}
