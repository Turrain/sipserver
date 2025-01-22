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
        bool watch_for_changes = false) :
        m_filename(filename),
        m_default_config(default_config),
        m_watch(watch_for_changes)
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
    T get(const std::string &key, bool check_env = true) const
    {
        std::lock_guard lock(m_mutex);

        // Environment variables
        if (check_env) {
            if (auto env_val = get_env_value(key); !env_val.empty()) {
                return convert_value<T>(env_val);
            }
        }

        // Command-line arguments
        if (auto cli_val = get_cli_value(key); !cli_val.empty()) {
            return convert_value<T>(cli_val);
        }

        // Configuration file or defaults
        try {
            return get_value<T>(key);
        } catch (const std::exception &) {
            return m_default_config.value<T>(key, T {});
        }
    }

  template<typename T>
    void set(const std::string& key, const T& value, bool trigger_observers = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        json* current = &m_config;
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

    // Batch operations
    void merge_patch(const json &patch)
    {
        std::lock_guard lock(m_mutex);
        m_config.merge_patch(patch);
        notify_observers("*");
    }

   void bulk_set(const std::vector<std::pair<std::string, json>>& updates) {
        std::unique_lock<std::mutex> lock(m_mutex);
        std::vector<std::string> changed_keys;
        
        for (const auto& [key_path, value] : updates) {
            try {
                // Use internal set without observers
                json* current = &m_config;
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
            } catch (const std::exception& e) {
                std::cerr << "Bulk set failed for " << key_path 
                        << ": " << e.what() << "\n";
            }
        }
        
        lock.unlock();
        for (const auto& key : changed_keys) {
            notify_observers(key);
        }
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
        std::lock_guard lock(m_mutex);
        try {
            fs::path temp_path = m_filename + ".tmp";
            
            // Write to temp file
            {
                std::ofstream file(temp_path);
                if (!file) {
                    throw std::runtime_error("Failed to create temporary config file at " + temp_path.string());
                }
                file << m_config.dump(4);
            }

            // Ensure parent directory exists
            fs::create_directories(temp_path.parent_path());

            // Atomic replace
            fs::rename(temp_path, m_filename);
            
            // Verify write succeeded
            if (!fs::exists(m_filename) || fs::file_size(m_filename) == 0) {
                throw std::runtime_error("Atomic save verification failed for " + m_filename);
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Config save filesystem error: " << e.what() << "\n";
            throw;
        } catch (const std::exception& e) {
            std::cerr << "Config save error: " << e.what() << "\n";
            throw;
        }
    }

    bool reload()
    {
        std::lock_guard lock(m_mutex);
        return load_config();
    }

    // Utility methods
    void parse_command_line(int argc, char *argv[])
    {
        std::lock_guard lock(m_mutex);
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.substr(0, 2) == "--") {
                size_t pos = arg.find('=');
                if (pos != std::string::npos) {
                    std::string key = arg.substr(2, pos - 2);
                    std::string value = arg.substr(pos + 1);
                    set(key, value, false);
                }
            }
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

private:
    void initialize()
    {
        load_config();
       
        m_config.merge_patch(m_default_config);
        if (m_watch) {
            start_file_watcher();
        }
    }

    bool load_config()
    {
        try {
            if (!fs::exists(m_filename)) {
                // Create parent directories if needed
                fs::create_directories(fs::path(m_filename).parent_path());
                
                // Write default config if file doesn't exist
                std::ofstream outfile(m_filename);
                if (outfile) {
                    outfile << m_default_config.dump(4);
                }
            }

            std::ifstream file(m_filename);
            if (file) {
                try {
                    json new_config = json::parse(file);
                    // Deep merge with validation
                    if (!new_config.is_object()) {
                        throw std::runtime_error("Root config must be a JSON object");
                    }
                    m_config = m_default_config;  // Reset to defaults
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
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Config load error: " << e.what() << "\n";
        }
        
        return false;
    }

    void start_file_watcher()
    {
        m_watcher_thread = std::thread([this]() {
            auto last_write = fs::last_write_time(m_filename);
            while (!m_stop_watch) {
                std::this_thread::sleep_for(1s);
                try {
                    auto current_write = fs::last_write_time(m_filename);
                    if (current_write != last_write) {
                        last_write = current_write;
                        reload();
                        notify_observers("*");
                    }
                } catch (...) {
                }
            }
        });
    }

    void notify_observers(const std::string& changed_key) {
        std::vector<ObserverCallback> callbacks;
        json config_copy;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            config_copy = m_config;
            
            for (const auto& [key, cb_list] : m_observers) {
                if (key == changed_key || 
                   (key.size() > 1 && key.back() == '*' && 
                    changed_key.compare(0, key.size()-1, key, 0, key.size()-1) == 0) ||
                    key == "*") {
                    callbacks.insert(callbacks.end(), cb_list.begin(), cb_list.end());
                }
            }
        }
        
        for (const auto& callback : callbacks) {
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

    // Member variables
    mutable std::mutex m_mutex;
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

} // namespace core
