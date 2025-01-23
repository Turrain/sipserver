#pragma once
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deps/json.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;
using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace core {

class Configuration {
public:
    using ObserverCallback = std::function<void(const json &)>;

    // Constructor
    Configuration(const std::string &filename,
        const json &default_config = {},
        bool watch_for_changes = false,
        bool enable_file_ops = true) :
        m_filename(filename),
        m_default_config(default_config),
        m_watch(watch_for_changes),
        m_enable_file_ops(enable_file_ops)
    {
        initialize();
    }

    // Destructor
    ~Configuration()
    {
        if (m_watch) {
            m_stop_watch = true;
            if (m_watcher_thread.joinable()) {
                m_watcher_thread.join();
            }
        }
    }

    // Core access methods
    template<typename T>
    T get(const std::string &key, bool check_env = false, bool check_cli = false) const
    {
        std::lock_guard lock(m_mutex);

        // Environment variables
        if (check_env) {
            if (auto env_val = get_env_value(key); !env_val.empty()) {
                return convert_value<T>(env_val);
            }
        }

        // Command-line arguments
        if (check_cli) {
            if (auto cli_val = get_cli_value(key); !cli_val.empty()) {
                return convert_value<T>(cli_val);
            }
        }

        // Configuration file or defaults
        try {
            return get_value<T>(key);
        } catch (const std::exception &) {
            try {
                json::json_pointer ptr(convert_key(key));
                return m_default_config.at(ptr).get<T>();
            } catch (const std::exception &) {
                return T {}; // Fallback to default-constructed value
            }
        }
    }
    template<typename T>
    void set(const std::string &key, const T &value, bool trigger_observers = true)
    {
        std::unique_lock lock(m_mutex);
        json *current = &m_config;
        std::vector<std::string> tokens;
        std::string token;

        // Split key by dots
        std::istringstream iss(key);
        while (std::getline(iss, token, '.')) {
            tokens.push_back(token);
        }

        // Navigate through JSON structure
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i == tokens.size() - 1) {
                (*current)[tokens[i]] = value;
            } else {
                current = &(*current)[tokens[i]];
            }
        }

        if (trigger_observers) {
            notify_observers(key);
        }
    }

    void bulk_set(const std::vector<std::pair<std::string, json>> &updates)
    {
        std::unique_lock lock(m_mutex);
        std::vector<std::string> changed_keys;

        for (const auto &[key_path, value]: updates) {
            try {
                // Use internal set without observers
                json *current = &m_config;
                std::vector<std::string> tokens;
                std::string token;

                std::istringstream iss(key_path);
                while (std::getline(iss, token, '.')) {
                    tokens.push_back(token);
                }

                for (size_t i = 0; i < tokens.size(); ++i) {
                    if (i == tokens.size() - 1) {
                        (*current)[tokens[i]] = value;
                    } else {
                        current = &(*current)[tokens[i]];
                    }
                }

                changed_keys.push_back(key_path);
            } catch (const std::exception &e) {
                std::cerr << "Bulk set failed for " << key_path
                          << ": " << e.what() << "\n";
            }
        }

        lock.unlock();
        for (const auto &key: changed_keys) {
            notify_observers(key);
        }
    }

    class JsonTransaction {
    public:
        JsonTransaction(Configuration &config, const json &patch) :
            m_config(config), m_patch(patch), m_active(true)
        {
            m_config.m_mutex.lock();
            m_original = m_config.m_config;
        }

        void commit()
        {
            if (m_active) {
                // Apply patch to original state
                json patched = m_original;
                patched.merge_patch(m_patch);

                // Validate before applying
                if (validate(patched)) {
                    m_config.m_config = patched;
                    m_config.notify_observers("*");
                }
                release();
            }
        }

        void rollback()
        {
            if (m_active) {
                release();
            }
        }

        ~JsonTransaction()
        {
            if (m_active)
                rollback();
        }

    private:
        Configuration &m_config;
        json m_patch;
        json m_original;
        bool m_active;

        bool validate(const json &proposed)
        {
            // Add custom validation logic here
            return true;
        }

        void release()
        {
            m_config.m_mutex.unlock();
            m_active = false;
        }
    };
    // New transaction methods
    JsonTransaction json_transaction(const json &patch)
    {
        return JsonTransaction(*this, patch);
    }

    void atomic_patch(const json &patch)
    {
        JsonTransaction txn(*this, patch);
        txn.commit();
    }

    // Modified merge_patch using transactions
    void merge_patch(const json &patch)
    {
        atomic_patch(patch);
    }

    // Transaction support
    class Transaction {
    public:
        Transaction(Configuration &config) :
            m_config(config), m_locked(true)
        {
            m_config.m_mutex.lock();
            m_snapshot = m_config.m_config;
        }

        void commit()
        {
            if (m_locked) {
                m_config.m_mutex.unlock();
                m_locked = false;
                m_config.notify_observers("*");
            }
        }

        void rollback()
        {
            if (m_locked) {
                m_config.m_config = m_snapshot;
                m_config.m_mutex.unlock();
                m_locked = false;
            }
        }

        json &data() { return m_config.m_config; }

        ~Transaction()
        {
            if (m_locked) {
                m_config.m_mutex.unlock();
            }
        }

    private:
        Configuration &m_config;
        json m_snapshot;
        bool m_locked;
    };

    // Provider management
    void update_providers(std::function<void(json &)> modifier)
    {
        std::lock_guard lock(m_mutex);
        modifier(m_config["providers"]);
        notify_observers("providers");
    }

    void set_provider_field(const std::string &provider,
        const std::string &field,
        const json &value)
    {
        std::lock_guard lock(m_mutex);
        m_config["providers"][provider][field] = value;
        notify_observers("providers." + provider);
    }

    void bulk_update_providers(const std::string &field,
        const json &values)
    {
        std::lock_guard lock(m_mutex);
        for (auto &[provider, config]: m_config["providers"].items()) {
            if (values.contains(provider)) {
                config[field] = values[provider];
            }
        }
        notify_observers("providers");
    }

    // Observers
    void add_observer(const std::string &key, ObserverCallback callback)
    {
        std::lock_guard lock(m_mutex);
        m_observers[key].push_back(callback);
    }

    // File operations
    void atomic_save() const
    {
        if (!m_enable_file_ops) {
            return; // No-op if file operations are disabled
        }

        std::lock_guard lock(m_mutex);
        try {
            fs::path config_path = fs::path(m_filename);
            fs::path temp_path = config_path.parent_path() / (config_path.filename().string() + ".tmp");

            // Ensure parent directory exists before any file operations
            if (!config_path.parent_path().empty()) {
                fs::create_directories(config_path.parent_path());
            }

            // Write to temp file
            {
                std::ofstream file(temp_path);
                if (!file) {
                    throw std::runtime_error("Failed to create temporary config file at " + temp_path.string());
                }
                file << m_config.dump(4);
                file.close(); // Ensure file is properly closed
            }

            // Atomic replace
            fs::rename(temp_path, config_path);

            // Verify write succeeded
            if (!fs::exists(config_path) || fs::file_size(config_path) == 0) {
                throw std::runtime_error("Atomic save verification failed for " + config_path.string());
            }
        } catch (const fs::filesystem_error &e) {
            std::cerr << "Config save filesystem error: " << e.what() << "\n";
            throw;
        } catch (const std::exception &e) {
            std::cerr << "Config save error: " << e.what() << "\n";
            throw;
        }
    }

    bool reload()
    {
        if (!m_enable_file_ops) {
            return false;
        }
        std::lock_guard lock(m_mutex);
        return load_config();
    }

    // Utility methods
    void parse_command_line(int argc, char *argv[])
    {
        std::vector<std::pair<std::string, json>> cli_updates;
        std::string current_key;

        // Helper function to process collected key-value pairs
        auto process_pair = [&](const std::string &key, const std::string &raw_value) {
            try {
                // Convert CLI key to JSON pointer format
                std::string json_key = std::regex_replace(key, std::regex { "\\[([0-9]+)\\]" }, ".$1");
                json_key = std::regex_replace(json_key, std::regex { "\\.\\.+" }, ".");

                // Handle boolean flags
                if (raw_value.empty()) {
                    if (key.find("no-") == 0) {
                        cli_updates.emplace_back(key.substr(3), false);
                    } else {
                        cli_updates.emplace_back(key, true);
                    }
                    return;
                }

                // Parse value with type detection
                json value;
                if (raw_value == "true")
                    value = true;
                else if (raw_value == "false")
                    value = false;
                else if (raw_value == "null")
                    value = nullptr;
                else if (std::regex_match(raw_value, std::regex { "^-?\\d+$" }))
                    value = std::stoi(raw_value);
                else if (std::regex_match(raw_value, std::regex { "^-?\\d+\\.\\d+$" }))
                    value = std::stod(raw_value);
                else if (raw_value.front() == '[' && raw_value.back() == ']') {
                    value = json::parse(raw_value);
                } else if (raw_value.front() == '{' && raw_value.back() == '}') {
                    value = json::parse(raw_value);
                } else {
                    value = raw_value;
                }

                cli_updates.emplace_back(json_key, value);
            } catch (const std::exception &e) {
                std::cerr << "Error parsing CLI argument " << key
                          << "=" << raw_value << ": " << e.what() << "\n";
            }
        };

        // Main parsing loop
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg.substr(0, 2) == "--") {
                // Finish previous key if needed
                if (!current_key.empty()) {
                    process_pair(current_key.substr(2), "");
                    current_key.clear();
                }

                size_t eq_pos = arg.find('=');
                if (eq_pos != std::string::npos) {
                    // Key-value pair with =
                    std::string key = arg.substr(2, eq_pos - 2);
                    std::string value = arg.substr(eq_pos + 1);
                    process_pair(key, value);
                } else {
                    // Potential boolean flag or separated value
                    current_key = arg;
                }
            } else if (!current_key.empty()) {
                // Value for previous key
                process_pair(current_key.substr(2), arg);
                current_key.clear();
            }
        }

        // Process remaining key without value
        if (!current_key.empty()) {
            process_pair(current_key.substr(2), "");
        }

        // Apply all updates in one atomic operation
        if (!cli_updates.empty()) {
            bulk_set(cli_updates);
        }
    }

    void reset(const std::string &path = "")
    {
        std::lock_guard lock(m_mutex);
        if (path.empty()) {
            m_config = m_default_config;
        } else {
            json::json_pointer ptr(convert_key(path));
            m_config[ptr] = m_default_config[ptr];
        }
        notify_observers(path.empty() ? "*" : path);
    }

    json get_json() const
    {
        std::lock_guard lock(m_mutex);
        return m_config;
    }

    // Create a new Configuration instance with a scoped view of this configuration
    std::shared_ptr<Configuration> create_scoped_config(const std::string &scope)
    {
        std::lock_guard lock(m_mutex);
        json scoped_defaults;
        try {
            json::json_pointer ptr(convert_key(scope));
            if (m_config.contains(ptr)) {
                scoped_defaults = m_config[ptr];
            }
        } catch (...) {
            // If path doesn't exist, start with empty defaults
        }

        auto scoped_config = std::make_shared<Configuration>(
            m_filename + "." + scope, // Unique filename for the scoped config
            scoped_defaults,
            false,
            false // Don't watch the file since parent handles that
        );

        // Add observer to parent to update scoped config
        add_observer(scope, [scoped_config, scope](const json &updated) {
            if (updated.contains(scope)) {
                scoped_config->merge_patch(updated[scope]);
            }
        });

        return scoped_config;
    }

private:
    void initialize()
    {
        if (m_enable_file_ops) {
            load_config();
        }
        m_config.merge_patch(m_default_config);
        if (m_watch && m_enable_file_ops) { // Ensure watcher respects the flag
            start_file_watcher();
        }
    }

    bool load_config()
    {
        try {
            fs::path config_path = fs::path(m_filename);

            if (!fs::exists(config_path)) {
                // Create parent directory if needed and it doesn't exist
                if (!config_path.parent_path().empty()) {
                    fs::create_directories(config_path.parent_path());
                }

                // Write default config if file doesn't exist
                std::ofstream outfile(config_path);
                if (outfile) {
                    outfile << m_default_config.dump(4);
                    outfile.close();
                }
            }

            std::ifstream file(config_path);
            if (file) {
                try {
                    json new_config = json::parse(file);
                    // Deep merge with validation
                    if (!new_config.is_object()) {
                        throw std::runtime_error("Root config must be a JSON object");
                    }
                    m_config = m_default_config; // Reset to defaults
                    m_config.merge_patch(new_config);
                    return true;
                } catch (const json::parse_error &e) {
                    std::cerr << "Config parse error [" << m_filename << "]: "
                              << e.what() << "\n";
                    std::cerr << "Falling back to default configuration\n";
                    m_config = m_default_config;
                    return false;
                }
            }
        } catch (const fs::filesystem_error &e) {
            std::cerr << "Filesystem error: " << e.what() << "\n";
        } catch (const std::exception &e) {
            std::cerr << "Config load error: " << e.what() << "\n";
        }

        return false;
    }

    void start_file_watcher()
    {
        m_watcher_thread = std::thread([this]() {
            fs::path config_path = fs::path(m_filename);

            // Initial check for file existence
            if (!fs::exists(config_path)) {
                return;
            }

            auto last_write = fs::last_write_time(config_path);
            while (!m_stop_watch) {
                std::this_thread::sleep_for(1s);
                try {
                    if (!fs::exists(config_path)) {
                        continue;
                    }
                    auto current_write = fs::last_write_time(config_path);
                    if (current_write != last_write) {
                        last_write = current_write;
                        reload();
                        notify_observers("*");
                    }
                } catch (const fs::filesystem_error &e) {
                    std::cerr << "File watcher error: " << e.what() << "\n";
                } catch (const std::exception &e) {
                    std::cerr << "Unexpected error in file watcher: " << e.what() << "\n";
                }
            }
        });
    }

    void notify_observers(const std::string &changed_key)
    {
        std::vector<ObserverCallback> callbacks;
        json config_copy;

        {
            std::unique_lock lock(m_mutex);
            config_copy = m_config;

            for (const auto &[key, cb_list]: m_observers) {
                if (key == changed_key || (key.size() > 1 && key.back() == '*' && changed_key.compare(0, key.size() - 1, key, 0, key.size() - 1) == 0) || key == "*") {
                    callbacks.insert(callbacks.end(), cb_list.begin(), cb_list.end());
                }
            }
        }

        for (const auto &callback: callbacks) {
            callback(config_copy);
        }
    }

    std::string get_env_value(const std::string &key) const
    {
        std::string env_var;
        for (char c: key) {
            env_var += (c == '.') ? '_' : std::toupper(c);
        }
        if (const char *val = std::getenv(env_var.c_str())) {
            return val;
        }
        return {};
    }

    std::string get_cli_value(const std::string &key) const
    {
        try {
            return m_config.at("__cli__").value(key, "");
        } catch (...) {
            return "";
        }
    }

    template<typename T>
    T convert_value(const std::string &str) const
    {
        std::istringstream iss(str);
        T value;
        if (!(iss >> value))
            throw std::runtime_error("Conversion failed");
        return value;
    }

    template<typename T>
    T get_value(const std::string &key) const
    {
        json::json_pointer ptr(convert_key(key));
        if (!m_config.contains(ptr)) {
            throw std::out_of_range("Key not found: " + key);
        }
        return m_config.at(ptr).get<T>();
    }

    static std::string convert_key(const std::string &key)
    {
        return "/" + std::regex_replace(key, std::regex { "\\." }, "/");
    }
    bool m_enable_file_ops;
    // Member variables
    mutable std::recursive_mutex m_mutex;
    json m_config;
    json m_default_config;
    std::string m_filename;
    bool m_watch = false;
    std::thread m_watcher_thread;
    std::atomic<bool> m_stop_watch { false };
    std::unordered_map<std::string, std::vector<ObserverCallback>> m_observers;
};

// Template specializations
template<>
inline std::string Configuration::convert_value<std::string>(const std::string &str) const
{
    return str;
}

template<>
inline bool Configuration::convert_value<bool>(const std::string &str) const
{
    std::istringstream iss(str);
    bool value;
    iss >> std::boolalpha >> value;
    return value;
}

template<>
inline std::vector<std::string> Configuration::convert_value<std::vector<std::string>>(const std::string &str) const
{
    std::vector<std::string> result;
    json j = json::parse(str);
    if (j.is_array()) {
        return j.get<std::vector<std::string>>();
    }
    throw std::runtime_error("Cannot convert to vector<string>");
}

} // namespace core
