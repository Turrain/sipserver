#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "agent/agent.h"
#include "core/configuration.h"
#include "db/GlobalDatabase.h"
#include "provider/provider_manager.h"
#include "server/server.h"
#include "utils/logger.h"
// Function implementations
#include <execinfo.h>
#include <csignal>
#include <boost/stacktrace.hpp>

// Function to print the current stack trace
void print_stacktrace() {
    // Boost automatically demangles symbols if -rdynamic is provided when linking.
    std::cout << boost::stacktrace::stacktrace() << std::endl;
}

// Terminate handler: called when an unhandled exception is thrown.
void my_terminate() {
    std::cerr << "\nUnhandled exception! Stack trace:" << std::endl;
    print_stacktrace();
    std::abort(); // Optionally generate a core dump.
}

// Signal handler: catches signals like SIGSEGV and SIGABRT.
void signal_handler(int signal) {
    std::cerr << "\nCaught signal " << signal << ". Stack trace:" << std::endl;
    print_stacktrace();
    boost::stacktrace::safe_dump_to("./backtrace.dump");
    std::exit(signal);
}


static void runTestMode()
{
    // Initialize components
    ProviderManager::getInstance().load_providers_from_folder("./lua");
    auto result = ProviderManager::getInstance().process_request(
        "ollama",
        "Explain the difference between RAG and fine-tuning",
        {
            { "temperature", 0.5 },
            { "model", "llama3.2:1b" },
        },
        { { { "role", "system" }, { "content", "system_prompt" } } });

    LOG_DEBUG << result.response;
}

int main(int argc, char **argv)
{
     std::set_terminate(my_terminate);

    // Register signal handlers for fatal signals.
    std::signal(SIGSEGV, signal_handler); // Segmentation fault
    std::signal(SIGABRT, signal_handler); // Abort signal
    std::signal(SIGFPE,  signal_handler); // floating point exception
    std::signal(SIGILL,  signal_handler); // illegal instruction
    std::signal(SIGTERM, signal_handler); // termination request
    std::signal(SIGINT,  signal_handler); // interrupt from keyboard


    Logger::setMinLevel(Level::Debug);
    AppConfig &config = AppConfig::getInstance();
    config.add_options({ { "test", "t", CLIParser::Type::Boolean, "test", "false" } });
    config.initialize(argc, argv);
    GlobalDatabase::instance().configureAutoPersist("db_backup.bson");
    GlobalDatabase::instance().configurePersistStrategy(true);
    GlobalDatabase::instance().initialize("");
    const bool test_mode = config.get<bool>("test");
    try {

        if (test_mode) {
            LOG_CRITICAL << "Test mode enabled";
            runTestMode();
        } else {
            Server server;
            server.run();
        }
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
