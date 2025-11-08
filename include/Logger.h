#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};


class Logger {
public:
    // Get singleton instance
    static Logger& getInstance();

    // Set the minimum log level (default: INFO)
    void setLogLevel(LogLevel level);

    // Log a message at specified level
    void log(LogLevel level, const std::string& message);

    // Helper methods for each level
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger();  
    ~Logger();

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string levelToString(LogLevel level);
    std::string getCurrentTimestamp();
    
    LogLevel minLevel_;
    std::ofstream logFile_;
    std::mutex mutex_;  // Thread safety
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARN(msg) Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)