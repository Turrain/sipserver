#pragma once

#include <algorithm>
#include <atomic>
#include <deps/json.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <fstream>
using json = nlohmann::json;

namespace std {
template<>
struct hash<json::json_pointer> {
    size_t operator()(const json::json_pointer &p) const
    {
        return hash<string>()(p.to_string());
    }
};
} // namespace std

namespace core {

// Add hash specialization for json_pointer

class Configuration {
public:
    using Observer = std::function<void(const json::json_pointer &, const json &, const json &)>;

    Configuration() = default;

    void beginTransaction()
    {
        if (transaction_data_) {
            throw std::runtime_error("Transaction already active");
        }
        transaction_data_ = data_;
        modified_paths_.clear();
    }
    void loadFromFile(const std::string &filename, bool merge = false)
    {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Could not open file: " + filename);
            }

            json loaded = json::parse(file);

            if (transaction_data_) {
                handleLoad(*transaction_data_, loaded, merge);
                modified_paths_.clear(); // Consider all paths modified
            } else {
                handleLoad(data_, loaded, merge);
            }
        } catch (const std::exception &e) {
            throw std::runtime_error("Failed to load from file: " + std::string(e.what()));
        }
    }
    void saveToFile(const std::string &filename) const
    {
        try {
            std::ofstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Could not create file: " + filename);
            }

            const json &target = transaction_data_ ? *transaction_data_ : data_;
            file << target.dump(4); // Pretty print with 4-space indentation
        } catch (const std::exception &e) {
            throw std::runtime_error("Failed to save to file: " + std::string(e.what()));
        }
    }

     const json& getData() const noexcept {
        return transaction_data_ ? *transaction_data_ : data_;
    }

    // Get scoped data (const version)
    json getData(const json::json_pointer& scope) const {
        return get(scope);
    }

    // Get scoped data (string version)
    json getData(const std::string& scope) const {
        return getData(json::json_pointer(scope));
    }

    void handleLoad(json &target, const json &loaded, bool merge)
    {
        if (merge) {
            target.merge_patch(loaded);
        } else {
            target = loaded;
        }
    }
    void commit()
    {
        if (!transaction_data_) {
            throw std::runtime_error("No active transaction to commit");
        }

        for (const auto &path: modified_paths_) {
            json old_value;
            try {
                old_value = data_.at(path);
            } catch (...) {
                old_value = nullptr;
            }

            json new_value;
            try {
                new_value = transaction_data_->at(path);
            } catch (...) {
                new_value = nullptr;
            }

            notifyObservers(path, old_value, new_value);
        }

        data_ = std::move(*transaction_data_);
        transaction_data_.reset();
        modified_paths_.clear();
    }

    void rollback()
    {
        if (!transaction_data_) {
            throw std::runtime_error("No active transaction to rollback");
        }
        transaction_data_.reset();
        modified_paths_.clear();
    }
     template<typename T>
    T get(const json::json_pointer& path, const T& default_value = T{}) const {
        const json& target = transaction_data_ ? *transaction_data_ : data_;
        return target.value(path, default_value);
    }

    // Overload for string paths
    template<typename T>
    T get(const std::string& path, const T& default_value = T{}) const {
        return get<T>(json::json_pointer(path), default_value);
    }

    json get(const std::string& path){
        return get(json::json_pointer(path));
    }
    json get(const json::json_pointer &path) const
    {
        const json &target = transaction_data_ ? *transaction_data_ : data_;

        // Check if path exists and is not null
        if (target.contains(path) && !target[path].is_null()) {
            return target[path];
        }
        return json(); // Return null JSON value as default
    }

    void set(const json::json_pointer &path, const json &value)
    {
        if (transaction_data_) {
            ensurePathExists(*transaction_data_, path);
            (*transaction_data_)[path] = value;
            modified_paths_.insert(path);
        } else {
            ensurePathExists(data_, path);
            json old_value = data_.value(path, json());
            data_[path] = value;
            notifyObservers(path, old_value, value);
        }
    }
    void set(const std::string &path, const json &value)
    {
        set(json::json_pointer(path), value);
    }
    void registerObserver(const json::json_pointer &path, Observer observer)
    {
        observers_.emplace_back(path, std::move(observer));
    }

    // Overload for string paths
    void registerObserver(const std::string &path, Observer observer)
    {
        registerObserver(json::json_pointer(path), std::move(observer));
    }

private:
    void ensurePathExists(json &j, const json::json_pointer &path)
    {
        json *current = &j;
        std::string ptr = path.to_string();
        std::istringstream iss(ptr);
        std::string token;

        // Skip empty first token if path starts with '/'
        if (!ptr.empty() && ptr[0] == '/')
            iss.get();

        while (std::getline(iss, token, '/')) {
            if (!current->contains(token)) {
                (*current)[token] = json::object();
            }
            current = &(*current)[token];
        }
    }

    void notifyObservers(const json::json_pointer &path,
        const json &old_value, const json &new_value)
    {
        for (const auto &[observer_path, observer]: observers_) {
            if (isPathObserved(path, observer_path)) {
                observer(path, old_value, new_value);
            }
        }
    }

    static bool isPathObserved(const json::json_pointer &changed,
        const json::json_pointer &observer)
    {
        const std::string cs = changed.to_string();
        const std::string os = observer.to_string();

        if (os == "/")
            return true;
        if (cs == os)
            return true;

        return (cs.length() > os.length()) && (cs.compare(0, os.length(), os) == 0) && (cs[os.length()] == '/');
    }

    json data_;
    std::optional<json> transaction_data_;
    std::unordered_set<json::json_pointer> modified_paths_;
    std::vector<std::pair<json::json_pointer, Observer>> observers_;
};

class ScopedConfiguration {
public:
    ScopedConfiguration(Configuration &core, const json::json_pointer &scope) :
        core_(core), scope_(scope)
    {
        if (!scope.empty()) {
            core_.set(scope, core_.get(scope));
        }
    }

    ScopedConfiguration(Configuration &core, const std::string &scope) :
        ScopedConfiguration(core, json::json_pointer(scope)) { }
    
    ScopedConfiguration(const ScopedConfiguration& other, const json::json_pointer &scope) :
        ScopedConfiguration(other.core_, scope / other.scope_) { }

  ScopedConfiguration(const ScopedConfiguration& other, const std::string &scope) :
        ScopedConfiguration(other.core_, json::json_pointer(scope)) { }

    void beginTransaction() { core_.beginTransaction(); }
    void commit() { core_.commit(); }
    void rollback() { core_.rollback(); }

    json get(const json::json_pointer &path) const
    {
        return core_.get(scope_ / path);
    }

    json get(const std::string &path) const
    {
        return get(json::json_pointer(path));
    }

     template<typename T>
    T get(const json::json_pointer& path, const T& default_value = T{}) const {
        return core_.get<T>(scope_ / path, default_value);
    }

    template<typename T>
    T get(const std::string& path, const T& default_value = T{}) const {
        return get<T>(json::json_pointer(path), default_value);
    }


    void set(const json::json_pointer &path, const json &value)
    {
        core_.set(scope_ / path, value);
    }

    void set(const std::string &path, const json &value)
    {
        set(json::json_pointer(path), value);
    }

        json getData() const {
        return core_.get(scope_);
    }

    // Get relative data within scope
    json getRelativeData(const json::json_pointer& path) const {
        return core_.get(scope_ / path);
    }

    json getRelativeData(const std::string& path) const {
        return getRelativeData(json::json_pointer(path));
    }

    void registerObserver(const json::json_pointer &path,
        Configuration::Observer observer)
    {
        core_.registerObserver(scope_ / path, std::move(observer));
    }

    void registerObserver(const std::string &path,
        Configuration::Observer observer)
    {
        registerObserver(json::json_pointer(path), std::move(observer));
    }

private:
    Configuration &core_;
    json::json_pointer scope_;
};

struct CLIArgument {
    enum class Type { Boolean,
        String,
        Number,
        Positional };
    std::string long_name;
    std::string short_name;
    Type type;
    std::string description;
    bool required = false;
    nlohmann::json default_value = nullptr;
    size_t position = 0; // For positional arguments

    CLIArgument(std::string ln, std::string sn, Type t, std::string desc,
        bool req = false, nlohmann::json def = nullptr, size_t pos = 0) :
        long_name(std::move(ln)),
        short_name(std::move(sn)), type(t),
        description(std::move(desc)), required(req), default_value(std::move(def)),
        position(pos) { }
};

class CLIParser {
public:
    CLIParser(std::vector<CLIArgument> args, std::string app_description = "") :
        arguments_(std::move(args)), description_(std::move(app_description)) { }

    nlohmann::json parse(int argc, char **argv)
    {
        nlohmann::json result;
        std::vector<std::string> positional_args;
        std::string app_name = argv[0];

        // Handle help option first
        if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            print_help(app_name);
            exit(0);
        }

        // Parse arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg.rfind("--", 0) == 0) {
                process_long_option(arg, result, i, argc, argv);
            } else if (arg.rfind("-", 0) == 0) {
                process_short_option(arg, result, i, argc, argv);
            } else {
                positional_args.push_back(arg);
            }
        }

        // Process positional arguments
        handle_positionals(positional_args, result);

        // Validate required arguments and set defaults
        validate_and_set_defaults(result);

        return result;
    }

private:
    std::vector<CLIArgument> arguments_;
    std::string description_;

    void print_help(const std::string &app_name)
    {
        std::cout << "Usage: " << app_name << " [OPTIONS]";

        // Collect positional arguments
        std::vector<CLIArgument> positionals;
        for (const auto &arg: arguments_) {
            if (arg.type == CLIArgument::Type::Positional) {
                positionals.push_back(arg);
            }
        }
        std::sort(positionals.begin(), positionals.end(),
            [](const CLIArgument &a, const CLIArgument &b) { return a.position < b.position; });

        for (const auto &pos: positionals) {
            std::cout << " <" << pos.long_name << ">";
        }
        std::cout << "\n\n"
                  << description_ << "\n\nOptions:\n";

        // Print options
        for (const auto &arg: arguments_) {
            if (arg.type == CLIArgument::Type::Positional)
                continue;

            std::string option_str = "  ";
            if (!arg.short_name.empty()) {
                option_str += "-" + arg.short_name + ", ";
            }
            option_str += "--" + arg.long_name;

            switch (arg.type) {
                case CLIArgument::Type::Boolean:
                    option_str += " (flag)";
                    break;
                case CLIArgument::Type::Number:
                    option_str += "=<num>";
                    break;
                case CLIArgument::Type::String:
                    option_str += "=<str>";
                    break;
                default:
                    break;
            }

            printf("  %-30s %s", option_str.c_str(), arg.description.c_str());

            if (arg.required)
                std::cout << " [required]";
            if (!arg.default_value.is_null())
                std::cout << " (default: " << arg.default_value << ")";

            std::cout << "\n";
        }
    }

    void process_long_option(const std::string &arg, nlohmann::json &result, int &idx, int argc, char **argv)
    {
        std::string content = arg.substr(2);
        size_t eq_pos = content.find('=');
        std::string key = (eq_pos != std::string::npos) ? content.substr(0, eq_pos) : content;
        std::string value = (eq_pos != std::string::npos) ? content.substr(eq_pos + 1) : "";

        auto it = find_argument_by_long(key);
        if (it == arguments_.end()) {
            throw std::runtime_error("Unknown option: --" + key);
        }

        process_argument(*it, value, result, idx, argc, argv);
    }

    void process_short_option(const std::string &arg, nlohmann::json &result, int &idx, int argc, char **argv)
    {
        std::string content = arg.substr(1);
        size_t eq_pos = content.find('=');
        std::string key = (eq_pos != std::string::npos) ? content.substr(0, eq_pos) : content;
        std::string value = (eq_pos != std::string::npos) ? content.substr(eq_pos + 1) : "";

        auto it = find_argument_by_short(key);
        if (it == arguments_.end()) {
            throw std::runtime_error("Unknown option: -" + key);
        }

        process_argument(*it, value, result, idx, argc, argv);
    }

    void process_argument(const CLIArgument &arg, std::string value, nlohmann::json &result,
        int &idx, int argc, char **argv)
    {
        if (arg.type == CLIArgument::Type::Boolean) {
            if (value.empty()) {
                result[arg.long_name] = true;
            } else {
                if (value == "true")
                    result[arg.long_name] = true;
                else if (value == "false")
                    result[arg.long_name] = false;
                else
                    throw std::runtime_error("Invalid boolean value for --" + arg.long_name);
            }
        } else {
            if (value.empty()) {
                if (idx + 1 >= argc) {
                    throw std::runtime_error("Missing value for option: --" + arg.long_name);
                }
                value = argv[++idx];
            }

            try {
                switch (arg.type) {
                    case CLIArgument::Type::Number:
                        result[arg.long_name] = std::stod(value);
                        break;
                    case CLIArgument::Type::String:
                        result[arg.long_name] = value;
                        break;
                    default:
                        throw std::runtime_error("Unexpected option type");
                }
            } catch (...) {
                throw std::runtime_error("Invalid value for --" + arg.long_name + ": " + value);
            }
        }
    }

    void handle_positionals(std::vector<std::string> &positionals, nlohmann::json &result)
    {
        std::vector<CLIArgument> pos_args;
        for (const auto &arg: arguments_) {
            if (arg.type == CLIArgument::Type::Positional) {
                pos_args.push_back(arg);
            }
        }
        std::sort(pos_args.begin(), pos_args.end(),
            [](const CLIArgument &a, const CLIArgument &b) { return a.position < b.position; });

        if (positionals.size() < pos_args.size()) {
            throw std::runtime_error("Insufficient positional arguments");
        }

        for (size_t i = 0; i < pos_args.size(); ++i) {
            const auto &arg = pos_args[i];
            try {
                if (arg.type == CLIArgument::Type::Number) {
                    result[arg.long_name] = std::stod(positionals[i]);
                } else {
                    result[arg.long_name] = positionals[i];
                }
            } catch (...) {
                throw std::runtime_error("Invalid value for positional argument: " + arg.long_name);
            }
        }

        // Store extra positionals
        if (positionals.size() > pos_args.size()) {
            nlohmann::json extras = nlohmann::json::array();
            for (size_t i = pos_args.size(); i < positionals.size(); ++i) {
                extras.push_back(positionals[i]);
            }
            result["_extra_args"] = extras;
        }
    }

    void validate_and_set_defaults(nlohmann::json &result)
    {
        for (const auto &arg: arguments_) {
            if (arg.required && !result.contains(arg.long_name)) {
                throw std::runtime_error("Missing required argument: --" + arg.long_name);
            }
            if (!arg.default_value.is_null() && !result.contains(arg.long_name)) {
                result[arg.long_name] = arg.default_value;
            }
        }
    }

    std::vector<CLIArgument>::const_iterator find_argument_by_long(const std::string &name) const
    {
        return std::find_if(arguments_.begin(), arguments_.end(),
            [&](const CLIArgument &a) { return a.long_name == name && a.type != CLIArgument::Type::Positional; });
    }

    std::vector<CLIArgument>::const_iterator find_argument_by_short(const std::string &name) const
    {
        return std::find_if(arguments_.begin(), arguments_.end(),
            [&](const CLIArgument &a) { return a.short_name == name && a.type != CLIArgument::Type::Positional; });
    }
};

} // namespace core
