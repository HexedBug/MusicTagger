#pragma once

#include <string>

namespace musictagger {

std::string trimWhitespace(std::string text);
std::string normalizeString(std::string text);
std::string removeQuotes(std::string text);

}
