#include "i18n.h"

#include <cstdlib>
#include <fstream>
#include <map>

static std::map<std::string, std::string> table;
static std::string lang = "en";

static std::string chosen() {
    std::ifstream in("/home/player/.config/puppy/settings");
    for (std::string line; std::getline(in, line);)
        if (line.compare(0, 9, "language=") == 0 && line.size() > 9) return line.substr(9);
    const char* env = getenv("LANG");   // pt_BR.UTF-8
    std::string l = env ? env : "";
    l = l.substr(0, l.find('.'));
    for (char& c : l)
        if (c == '_') c = '-';
    return l.empty() || l == "C" || l == "POSIX" ? "en" : l;
}

void i18n::load(const std::string& appDir) {
    lang = chosen();
    if (lang.find('/') != std::string::npos) lang = "en";
    std::ifstream in(appDir + "/lang/" + lang + ".lang");
    for (std::string line; std::getline(in, line);) {
        const size_t eq = line.find(" = ");
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 3);
        if (key != "language" && key != "language_en" && !value.empty()) table[key] = value;
    }
}

const std::string& i18n::code() { return lang; }

const char* tr(const char* english) {
    auto it = table.find(english);
    return it == table.end() ? english : it->second.c_str();
}

std::string tr(const std::string& english) {
    auto it = table.find(english);
    return it == table.end() ? english : it->second;
}

std::string trf(const char* english, const std::string& a, const std::string& b) {
    std::string text = tr(english);
    size_t pos = text.find("%s");
    if (pos != std::string::npos) {
        text.replace(pos, 2, a);
        pos = text.find("%s", pos + a.size());
        if (pos != std::string::npos) text.replace(pos, 2, b);
    }
    return text;
}
