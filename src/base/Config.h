#pragma once

#include <string>
#include <unordered_map>

namespace base {

class Config {
public:
    bool load(const std::string& path);

    std::string getString(const std::string& key, const std::string& default_value = "") const;
    int getInt(const std::string& key, int default_value = 0) const;

private:
    std::unordered_map<std::string, std::string> values_;
};

}  // namespace base
