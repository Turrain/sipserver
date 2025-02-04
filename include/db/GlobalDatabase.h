#pragma once
#include "db/InMemoryDatabase.h"
#include <atomic>
#include <mutex>
#include <shared_mutex>
class GlobalDatabase {
public:
    // Deleted copy/move operations
    GlobalDatabase(const GlobalDatabase &) = delete;
    GlobalDatabase &operator=(const GlobalDatabase &) = delete;
    GlobalDatabase(GlobalDatabase &&) = delete;
    GlobalDatabase &operator=(GlobalDatabase &&) = delete;

    // Singleton access
    static GlobalDatabase &instance()
    {
        static GlobalDatabase db;
        return db;
    }

    // Database operations with thread safety
    template<typename Func>
    decltype(auto) execute(Func &&func)
    {
        std::unique_lock lock(mutex_);

        if constexpr (std::is_void_v<decltype(func(database_))>) {
            func(database_);
            conditionalPersist();
            return;
        } else {
            auto &&result = func(database_);
            conditionalPersist();
            return std::forward<decltype(result)>(result);
        }
    }
    void configurePersistStrategy(bool immediate = true,
        std::chrono::seconds interval = {})
    {
        std::unique_lock lock(mutex_);
        immediate_persist_ = immediate;
        persist_interval_ = interval;
    }
    template<typename Func>
    auto query(Func &&func) const
    {
        std::shared_lock lock(mutex_);
        return func(database_);
    }
    void conditionalPersist()
    {
        if (auto_persist_ && !persist_path_.empty()) {
            const auto now = std::chrono::system_clock::now();
            const bool should_persist = immediate_persist_ || (persist_interval_.count() > 0 && (now - last_persist_) > persist_interval_);

            if (should_persist) {
                try {
                    database_.saveToFile(persist_path_);
                    last_persist_ = now;
                } catch (const std::exception &e) {
                    std::cerr << "Auto-persist failed: " << e.what() << "\n";
                }
            }
        }
    }
    // Lifecycle management
    void initialize(const std::string &config_path = "")
    {
        std::unique_lock lock(mutex_);
        if (initialized_)
            return;

        // Attempt to load persisted data first
        if (auto_persist_ && !persist_path_.empty()) {
            try {
                if (fs::exists(persist_path_)) {
                    database_.loadFromFile(persist_path_);
                    std::cout << "Loaded persisted database from: "
                              << persist_path_ << "\n";
                    initialized_ = true;
                    return;
                }
            } catch (const std::exception &e) {
                std::cerr << "Failed to load persisted data: "
                          << e.what() << "\n";
                database_.clear();
            }
        }

        // Load configuration if provided
        if (!config_path.empty()) {
            loadConfiguration(config_path);
        }

        // Initialize with default tables if empty
        if (database_.empty()) {
            initializeDefaultSchema();
        }

        initialized_ = true;
    }

    void shutdown() noexcept
    {
        std::unique_lock lock(mutex_);
        if (!initialized_)
            return;

        flushAllTransactions();

        if (auto_persist_ && !persist_path_.empty()) {
            try {
                database_.saveToFile(persist_path_);
                std::cout << "Final database state persisted to: "
                          << persist_path_ << "\n";
            } catch (const std::exception &e) {
                std::cerr << "Final persist failed: " << e.what() << "\n";
            }
        }

        initialized_ = false;
    }
    // Configuration management
    void configureAutoPersist(const std::string &path, bool enabled = true)
    {
        std::unique_lock lock(mutex_);
        persist_path_ = path;
        auto_persist_ = enabled;
    }

private:
    std::chrono::system_clock::time_point last_persist_;
    bool immediate_persist_ = true;
    std::chrono::seconds persist_interval_ { 0 };

    void loadFromDirectory(const std::string &path)
    {
        try {
            if (fs::exists(path) && fs::is_directory(path)) {
                database_.loadFromDirectory(path);
                std::cout << "Loaded database from directory: "
                          << path << "\n";
            }
        } catch (const std::exception &e) {
            std::cerr << "Directory load error: " << e.what() << "\n";
            throw;
        }
    }

    GlobalDatabase() = default;
    ~GlobalDatabase()
    {
        shutdown();
    }

    void loadConfiguration(const std::string &path)
    {
        try {
            std::ifstream config_file(path);
            if (!config_file) {
                throw std::runtime_error("Config file not found");
            }

            json config = json::parse(config_file);

            // Load directory paths if specified
            if (config.contains("data_directory")) {
                loadFromDirectory(config["data_directory"]);
            }

            // Load tables configuration
            if (config.contains("tables")) {
                for (const auto &[table_name, table_config]: config["tables"].items()) {
                    if (!database_.hasTable(table_name)) {
                        database_.createTable(table_name);
                    }

                    // Load initial data if specified
                    if (table_config.contains("initial_data")) {
                        auto &table = database_.getTable(table_name);
                        for (const auto &[doc_id, doc_data]: table_config["initial_data"].items()) {
                            table.insertDocument(doc_id, Document(doc_data));
                        }
                    }
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "Config load error: " << e.what() << "\n";
            throw;
        }
    }

    void initializeDefaultSchema()
    {
        // System tables
        database_.createTable("__metadata");
        database_.createTable("__transactions");
        database_.createTable("__audit_log");
    }

    void flushAllTransactions() noexcept
    {
        try {
            auto &transactions = database_.getTable("__transactions");
            transactions.deleteDocuments([](const Document &) { return true; });
        } catch (...) {
            // Log error but continue shutdown
        }
    }

    // Member variables
    mutable std::shared_mutex mutex_;
    InMemoryDatabase database_;
    std::atomic_bool initialized_ { false };
    std::string persist_path_;
    bool auto_persist_ = false;
};