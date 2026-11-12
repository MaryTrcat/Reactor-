#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace base {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

class Logger {
public:
    static Logger& instance();

    void setFile(const std::string& path);
    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;

    std::mutex mutex_;
    std::ofstream file_;
};

void logInfo(const std::string& message);
void logWarning(const std::string& message);
void logError(const std::string& message);

}  // namespace base
