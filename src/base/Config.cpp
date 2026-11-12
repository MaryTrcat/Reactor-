#include "base/Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace base {

namespace {

std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !is_space(ch); }).base(), value.end());
    return value;
}

}  // namespace

bool Config::load(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        const auto key = trim(line.substr(0, pos));
        const auto value = trim(line.substr(pos + 1));
        if (!key.empty()) {
            values_[key] = value;
        }
    }
    return true;
}

std::string Config::getString(const std::string& key, const std::string& default_value) const {
    const auto it = values_.find(key);
    return it == values_.end() ? default_value : it->second;
}

int Config::getInt(const std::string& key, int default_value) const {
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }

    try {
        return std::stoi(it->second);
    } catch (...) {
        return default_value;
    }
}

}  // namespace base
