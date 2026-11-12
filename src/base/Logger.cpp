#include "base/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace base {

namespace {

const char* levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "INFO";
}

std::string nowText() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.open(path, std::ios::app);
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostream& out = file_.is_open() ? file_ : std::cout;
    out << "[" << nowText() << "] [" << levelName(level) << "] " << message << '\n';
    out.flush();
}

void logInfo(const std::string& message) {
    Logger::instance().log(LogLevel::Info, message);
}

void logWarning(const std::string& message) {
    Logger::instance().log(LogLevel::Warning, message);
}

void logError(const std::string& message) {
    Logger::instance().log(LogLevel::Error, message);
}

}  // namespace base
