#pragma once
#include <iostream>
#include <map>
#include <string>
#include <ostream>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <fstream>

enum class LogLevel { DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL };

const std::map<::LogLevel, std::string> LogLevelColors = {
    { ::LogLevel::DEBUG, "\033[36m" }, // Cyan
    { ::LogLevel::INFO, "\033[32m" }, // Green
    { ::LogLevel::WARNING, "\033[33m" }, // Yellow
    { ::LogLevel::ERROR, "\033[31m" }, // Red
    { ::LogLevel::CRITICAL, "\033[41m" } // Red background
};

const std::string ResetColor = "\033[0m";

class Logger {
public:
    static Logger &getInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void setLogLevel(::LogLevel level) { logLevel = level; }

    void enableConsoleLogging(bool enable) { consoleLogging = enable; }

    void enableFileLogging(bool enable, const std::string &filename = "log.txt")
    {
        std::lock_guard<std::mutex> lock(mtx); // Ensure thread safety
        fileLogging = enable;
        if (fileLogging) {
            if (fileStream.is_open()) {
                fileStream.close();
            }
            fileStream.open(filename, std::ios::out | std::ios::app);
            if (!fileStream) {
                std::cerr << "Failed to open log file: " << filename << std::endl;
                fileLogging = false;
            }
        } else {
            if (fileStream.is_open()) {
                fileStream.close();
            }
        }
    }

    void log(::LogLevel level, const std::string &message)
    {
        if (level < logLevel) {
            return;
        }

        std::lock_guard<std::mutex> lock(mtx);

        std::string timestamp = getCurrentTimestamp();
        std::string levelStr = logLevelToString(level);
        std::string coloredLevelStr = getColoredLevel(level, levelStr);
        std::string coloredMessage = getColoredMessage(message, level);
        std::ostringstream oss;
        oss << "[" << timestamp << "] [" << coloredLevelStr << "] "
            << coloredMessage << "\n";
        std::string logMessage = oss.str();

        if (consoleLogging) {
            std::cout << logMessage;
        }

        if (fileLogging && fileStream.is_open()) {
            fileStream << "[" << timestamp << "] [" << levelStr << "] " << message
                       << "\n";
        }
    }

    // Existing logging methods accepting std::string
    void debug(const std::string &message) { log(::LogLevel::DEBUG, message); }
    void info(const std::string &message) { log(::LogLevel::INFO, message); }
    void warning(const std::string &message)
    {
        log(::LogLevel::WARNING, message);
    }
    void error(const std::string &message) { log(::LogLevel::ERROR, message); }
    void critical(const std::string &message)
    {
        log(::LogLevel::CRITICAL, message);
    }

    void debug(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string formattedMessage = formatString(format, args);
        va_end(args);
        log(::LogLevel::DEBUG, formattedMessage);
    }

    void info(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string formattedMessage = formatString(format, args);
        va_end(args);
        log(::LogLevel::INFO, formattedMessage);
    }

    void warning(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string formattedMessage = formatString(format, args);
        va_end(args);
        log(::LogLevel::WARNING, formattedMessage);
    }

    void error(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string formattedMessage = formatString(format, args);
        va_end(args);
        log(::LogLevel::ERROR, formattedMessage);
    }

    void critical(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string formattedMessage = formatString(format, args);
        va_end(args);
        log(::LogLevel::CRITICAL, formattedMessage);
    }

private:
    ::LogLevel logLevel;
    bool consoleLogging;
    bool fileLogging;
    std::ofstream fileStream;
    std::mutex mtx;
    Logger() :
        logLevel(::LogLevel::DEBUG), consoleLogging(true), fileLogging(false) { }

    std::string logLevelToString(::LogLevel level)
    {
        switch (level) {
            case ::LogLevel::DEBUG:
                return "DEBUG";
            case ::LogLevel::INFO:
                return "INFO";
            case ::LogLevel::WARNING:
                return "WARNING";
            case ::LogLevel::ERROR:
                return "ERROR";
            case ::LogLevel::CRITICAL:
                return "CRITICAL";
            default:
                return "UNKNOWN";
        }
    }

    std::string getColoredLevel(::LogLevel level, const std::string &levelStr)
    {
        auto it = LogLevelColors.find(level);
        if (it != LogLevelColors.end()) {
            return it->second + levelStr + ResetColor;
        }
        return levelStr;
    }

    std::string getColoredMessage(const std::string &message, ::LogLevel level)
    {
        auto it = LogLevelColors.find(level);
        if (it != LogLevelColors.end()) {
            return it->second + message + ResetColor;
        }
        return message;
    }

    std::string getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch())
            % 1000;

        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &timeT);
#else
        localtime_r(&timeT, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
            << std::setw(3) << milliseconds.count();
        return oss.str();
    }

    std::string formatString(const char *format, va_list args)
    {
        std::array<char, 1024> buffer;
        va_list argsCopy;
        va_copy(argsCopy, args);

        int length = std::vsnprintf(buffer.data(), buffer.size(), format, argsCopy);
        va_end(argsCopy);

        if (length < 0) {
            return "Formatting error";
        }

        if (static_cast<size_t>(length) < buffer.size()) {
            return std::string(buffer.data(), length);
        } else {
            std::string result(length, '\0');
            std::vsnprintf(&result[0], length + 1, format, args);
            return result;
        }
    }
};