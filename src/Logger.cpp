#include "Logger.h"
#include <iostream>
#include <filesystem>

Logger::Logger() : minLevel_(LogLevel::INFO) {
    // Get config directory path
    std::filesystem::path logDir;

    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            logDir = std::filesystem::path(appdata) / "DownloadManager";
        } else {
            logDir = ".";
        }
    #else
        const char* home = std::getenv("HOME");
        if (home) {
            logDir = std::filesystem::path(home) / ".config" / "DownloadManager";
        } else {
            logDir = ".";
        }
    #endif

    try {
        if (!std::filesystem::exists(logDir)) {
            std::filesystem::create_directories(logDir);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Warning: Could not create log directory: " << e.what() << std::endl;
        logDir = ".";  // Fall back to current directory
    }

    // Open log file in append mode
    std::filesystem::path logPath = logDir / "download.log";
    logFile_.open(logPath, std::ios::app);
    
    if (!logFile_.is_open()) {
        std::cerr << "Warning: Could not open log file: " << logPath << std::endl;
    } else {
        std::cout << "Logging to: " << logPath << std::endl;
    }
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

Logger& Logger::getInstance() {
    // Static local variable = thread-safe initialization (C++11+)
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    minLevel_ = level;
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    
    // Format: [2025-11-07 14:30:15]
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    // Skip if below minimum level
    if (level < minLevel_) {
        return;
    }
    
    // Thread-safe lock
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Format: [2025-11-07 14:30:15] [INFO] Download started
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);
    std::string formatted = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    // Write to stderr (immediate feedback)
    std::cerr << formatted << std::endl;
    
    // Write to file (persistent record)
    if (logFile_.is_open()) {
        logFile_ << formatted << std::endl;
        logFile_.flush();  // Ensure it's written immediately
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}