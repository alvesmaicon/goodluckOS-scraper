#pragma once
// The interface's language: language= in the launcher's settings (~/.config/puppy/settings), else LANG. Strings come
// from lang/<code>.lang in the app's folder, "English = translation" lines; English when one is missing.
#include <string>

namespace i18n {
void load(const std::string& appDir);
const std::string& code();     // e.g. pt-BR; en when none is chosen
}

const char* tr(const char* english);
std::string tr(const std::string& english);
std::string trf(const char* english, const std::string& a, const std::string& b = "");   // fills "%s" in order
