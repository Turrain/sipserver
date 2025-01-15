#pragma once
#include <chrono>
#include <cstdarg> // Required for variadic arguments (va_list, va_start, va_end)
#include <cstdio> // Required for vsnprintf
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

// For color output (platform-specific - works on Linux/macOS)
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

class Logger {
public:
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    // Set the log file (can be called to change the log file)
    static void setLogFile(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(logMutex);
        if (instance().logFile.is_open()) {
            instance().logFile.close();
        }
        instance().logToFile = false;
        if (!filename.empty()) {
            instance().logFile.open(filename, std::ios::app);
            if (instance().logFile.is_open()) {
                instance().logToFile = true;
            } else {
                std::cerr << "Error opening log file: " << filename << std::endl;
            }
        }
    }

    // Set whether to log to the console
    static void setConsoleOutput(bool enable)
    {
        instance().consoleOutput = enable;
    }

    // Log message with specified level and printf-style formatting
    static void log(LogLevel level, const char *format, ...)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime;

#ifdef _WIN32
        localtime_s(&localTime, &time);
#else
        localtime_r(&time, &localTime);
#endif

        // Format timestamp
        std::stringstream timestamp;
        timestamp << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");

        // Handle variadic arguments and format the message
        va_list args;
        va_start(args, format);

        // Determine the required buffer size
        va_list args_copy;
        va_copy(args_copy, args); // Copy va_list for second use
        int len = std::vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);

        // Create a buffer of sufficient size (+1 for null terminator)
        std::string formattedMessage;
        if (len > 0) {
            formattedMessage.resize(len + 1);
            std::vsnprintf(&formattedMessage[0], formattedMessage.size(), format, args);
            formattedMessage.resize(len); // Remove the extra null terminator added by vsnprintf if not at the end
        }

        va_end(args);

        // Format log level and message
        std::string levelStr = getLevelString(level);
        formattedMessage = "[" + timestamp.str() + "] [" + levelStr + "] " + formattedMessage;

        // Output to console with colors
        if (instance().consoleOutput) {
            outputToConsole(level, formattedMessage);
        }

        // Output to file
        if (instance().logToFile) {
            instance().logFile << formattedMessage << std::endl;
        }
    }

// Easy-to-use logging macros with formatting
#define LOG_DEBUG(format, ...) Logger::log(Logger::LogLevel::DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Logger::log(Logger::LogLevel::INFO, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) Logger::log(Logger::LogLevel::WARNING, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Logger::log(Logger::LogLevel::ERROR, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) Logger::log(Logger::LogLevel::FATAL, format, ##__VA_ARGS__)

private:
    // Private constructor (Singleton pattern)
    Logger() :
        logToFile(false), consoleOutput(true)
    {
#ifdef _WIN32
        // Get console handle for Windows
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    }

    // Private copy constructor and assignment operator (prevent copying)
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    // Static instance method (Singleton pattern)
    static Logger &instance()
    {
        static Logger instance;
        return instance;
    }

    // Destructor
    ~Logger()
    {
        if (logToFile) {
            logFile.close();
        }
    }

    // Helper function to convert log level to string
    static std::string getLevelString(LogLevel level)
    {
        switch (level) {
            case LogLevel::DEBUG:
                return "DEBUG";
            case LogLevel::INFO:
                return "INFO";
            case LogLevel::WARNING:
                return "WARNING";
            case LogLevel::ERROR:
                return "ERROR";
            case LogLevel::FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    // Helper functions for platform-specific colored output
    static void outputToConsole(LogLevel level, const std::string &message)
    {
#ifdef _WIN32
        // Windows console colors
        WORD color;
        switch (level) {
            case LogLevel::DEBUG:
                color = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break; // Bright Cyan
            case LogLevel::INFO:
                color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break; // Bright Green
            case LogLevel::WARNING:
                color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
                break; // Bright Yellow
            case LogLevel::ERROR:
                color = FOREGROUND_RED | FOREGROUND_INTENSITY;
                break; // Bright Red
            case LogLevel::FATAL:
                color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
                break; // Bright Magenta
            default:
                color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
                break; // White
        }
        SetConsoleTextAttribute(instance().hConsole, color);
        std::cout << message << std::endl;
        SetConsoleTextAttribute(instance().hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Reset to default color
#else
        // ANSI escape codes for Linux/macOS
        std::string colorCode;
        switch (level) {
            case LogLevel::DEBUG:
                colorCode = "\033[36m";
                break; // Cyan
            case LogLevel::INFO:
                colorCode = "\033[32m";
                break; // Green
            case LogLevel::WARNING:
                colorCode = "\033[33m";
                break; // Yellow
            case LogLevel::ERROR:
                colorCode = "\033[31m";
                break; // Red
            case LogLevel::FATAL:
                colorCode = "\033[35m";
                break; // Magenta
            default:
                colorCode = "\033[0m";
                break; // Reset
        }
        std::cout << colorCode << message << "\033[0m" << std::endl; // Reset color at the end
#endif
    }

private:
    std::ofstream logFile;
    bool logToFile;
    bool consoleOutput;
    static std::mutex logMutex;

#ifdef _WIN32
    HANDLE hConsole;
#endif
};
