#pragma once
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include "db/InMemoryDatabase.h"
class GlobalDatabase {
public:
    // Deleted copy/move operations
    GlobalDatabase(const GlobalDatabase&) = delete;
    GlobalDatabase& operator=(const GlobalDatabase&) = delete;
    GlobalDatabase(GlobalDatabase&&) = delete;
    GlobalDatabase& operator=(GlobalDatabase&&) = delete;

    // Singleton access
    static GlobalDatabase& instance() {
        static GlobalDatabase db;
        return db;
    }

    // Database operations with thread safety
    template<typename Func>
    auto execute(Func&& func) {
        std::unique_lock lock(mutex_);
        return func(database_);
    }

    template<typename Func>
    auto query(Func&& func) const {
        std::shared_lock lock(mutex_);
        return func(database_);
    }

    // Lifecycle management
    void initialize(const std::string& config_path = "") {
        std::unique_lock lock(mutex_);
        if (initialized_) return;
        
        // Load configuration
        if (!config_path.empty()) {
            loadConfiguration(config_path);
        }
        
        // Initialize with default tables if empty
        if (database_.empty()) {
            initializeDefaultSchema();
        }
        
        initialized_ = true;
    }

    void shutdown() noexcept {
        std::unique_lock lock(mutex_);
        if (!initialized_) return;
        
        // Flush pending operations
        flushAllTransactions();
        
        // Persist data if configured
        if (auto_persist_) {
            database_.saveToFile(persist_path_);
        }
        
        initialized_ = false;
    }

    // Configuration management
    void configureAutoPersist(const std::string& path, bool enabled = true) {
        std::unique_lock lock(mutex_);
        persist_path_ = path;
        auto_persist_ = enabled;
    }

private:
    GlobalDatabase() = default;
    ~GlobalDatabase() {
        shutdown();
    }

    void loadConfiguration(const std::string& path) {
        try {
            std::ifstream config_file(path);
            json config = json::parse(config_file);
            
            // Load tables and indexes
            for (const auto& [table_name, table_config] : config.items()) {
                auto& table = database_.createTable(table_name);
                
                // Configure indexes
              
            }
        } catch (const std::exception& e) {
            std::cerr << "Config load error: " << e.what() << "\n";
        }
    }

    void initializeDefaultSchema() {
        // System tables
        database_.createTable("__metadata");
        database_.createTable("__transactions");
        database_.createTable("__audit_log");
    }

    void flushAllTransactions() noexcept {
        try {
            auto& transactions = database_.getTable("__transactions");
            transactions.deleteDocuments([](const Document&) { return true; });
        } catch (...) {
            // Log error but continue shutdown
        }
    }

    // Member variables
    mutable std::shared_mutex mutex_;
    InMemoryDatabase database_;
    std::atomic_bool initialized_{false};
    std::string persist_path_;
    bool auto_persist_ = false;
};