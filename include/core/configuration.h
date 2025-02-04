#pragma once
#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
#if __has_include(<filesystem>)
    #define HAS_FILESYSTEM 1
    #include <filesystem>
    namespace fs = std::filesystem;
#endif
#endif

#ifndef HAS_FILESYSTEM
#include <sys/stat.h>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <optional>

namespace utils {
    [[nodiscard]] inline std::string trim(std::string_view s) {
        auto is_space = [](unsigned char c) { return std::isspace(c); };
        auto start = std::find_if_not(s.begin(), s.end(), is_space);
        auto end = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
        return std::string(start, end);
    }

    [[nodiscard]] inline constexpr bool starts_with(std::string_view str, std::string_view prefix) noexcept {
        return str.substr(0, prefix.size()) == prefix;
    }
    [[nodiscard]] inline bool file_exists(const std::string& filepath) {
        #if defined(HAS_FILESYSTEM)
                std::error_code ec;
                return fs::exists(filepath, ec);
        #else
                struct stat buffer;
                return (stat(filepath.c_str(), &buffer) == 0);
        #endif
    }
    // Case-insensitive string comparison
    struct CaseInsensitiveLess {
        bool operator()(std::string_view lhs, std::string_view rhs) const {
            return std::lexicographical_compare(
                lhs.begin(), lhs.end(),
                rhs.begin(), rhs.end(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) <
                           std::tolower(static_cast<unsigned char>(b));
                }
            );
        }
    };

    // Convert string to uppercase
    [[nodiscard]] inline std::string to_upper(std::string_view str) {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::toupper(c); });
        return result;
    }
}

class ConfigurationError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
class EnvParser {
public:
    [[nodiscard]] std::error_code load(const std::filesystem::path& filepath) noexcept {
        std::error_code ec;
        if (!std::filesystem::exists(filepath, ec)) {
            return ec;
        }

        std::ifstream file(filepath);
        if (!file) {
            return std::make_error_code(std::errc::io_error);
        }

        variables_.clear();
        std::string line;
        size_t line_number = 0;

        while (std::getline(file, line)) {
            ++line_number;
            try {
                process_line(line);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error processing line " << line_number
                         << ": " << e.what() << '\n';
            }
        }

        return {};
    }

    [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const noexcept {
        // Convert key to uppercase for case-insensitive lookup
        auto upper_key = utils::to_upper(key);
        if (auto it = variables_.find(upper_key); it != variables_.end()) {
            return std::string_view(it->second);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string_view get(std::string_view key,
                                      std::string_view default_value) const noexcept {
        return get(key).value_or(default_value);
    }

    [[nodiscard]] const auto& variables() const noexcept { return variables_; }

private:
    void process_line(std::string_view line) {
        std::string trimmed = utils::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') return;

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            throw ConfigurationError("Missing '=' in configuration line");
        }

        std::string key = utils::trim(trimmed.substr(0, eq_pos));
        std::string value = utils::trim(trimmed.substr(eq_pos + 1));

        if (key.empty()) {
            throw ConfigurationError("Empty key in configuration");
        }

        // Store keys in uppercase for case-insensitive lookup
        variables_.emplace(utils::to_upper(key), std::move(value));
    }

    std::map<std::string, std::string> variables_;
};


class CLIParser {
public:
    enum class Type {
        Boolean,
        String,
        Integer,
        Float
    };

    struct Option {
        std::string long_name;
        std::string short_name;
        Type type;
        std::string description;
        std::string default_value;
        bool required = false;
    };

    struct ParseResult {
        bool success;
        std::string error_message;
    };


    explicit CLIParser(std::vector<Option> options)
        : options_(std::move(options)) {
        for (const auto& opt : options_) {
            if (!opt.long_name.empty()) {
                option_map_[opt.long_name] = &opt;
            }
            if (!opt.short_name.empty()) {
                option_map_[opt.short_name] = &opt;
            }
        }
    }

    [[nodiscard]] ParseResult parse(int argc, char **  argv) {
        try {
            for (int i = 1; i < argc;) {
                std::string_view arg = argv[i];
                if (utils::starts_with(arg, "--")) {
                    i = handle_long_option(argc, argv, i);
                } else if (utils::starts_with(arg, "-")) {
                    i = handle_short_option(argc, argv, i);
                } else {
                    ++i;  // Skip positional arguments
                }
            }

            // Verify required options
            for (const auto& opt : options_) {
                if (opt.required && values_.find(opt.long_name) == values_.end()) {
                    return {false, "Missing required option: --" + opt.long_name};
                }
            }

            return {true, ""};
        } catch (const std::exception& e) {
            return {false, e.what()};
        }
    }

    [[nodiscard]] std::optional<std::string_view> get(std::string_view key) const noexcept {
        if (auto it = values_.find(std::string(key)); it != values_.end()) {
            return std::string_view(it->second);
        }
        if (auto opt_it = option_map_.find(std::string(key)); opt_it != option_map_.end()) {
            return std::string_view(opt_it->second->default_value);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string_view get(std::string_view key,
                                      std::string_view default_value) const noexcept {
        return get(key).value_or(default_value);
    }

    [[nodiscard]] bool get_bool(std::string_view key) const noexcept {
        auto value = get(key);
        return value && (*value == "true" || *value == "1");
    }

    void print_help() const {
        std::cout << "Options:\n";
        for (const auto& opt : options_) {
            std::cout << "  ";
            if (!opt.short_name.empty()) {
                std::cout << "-" << opt.short_name << ", ";
            }
            std::cout << "--" << opt.long_name;

            switch (opt.type) {
                case Type::String:
                    std::cout << " <string>";
                    break;
                case Type::Integer:
                    std::cout << " <int>";
                    break;
                case Type::Float:
                    std::cout << " <float>";
                    break;
                case Type::Boolean:
                    break;
            }

            std::cout << "\t" << opt.description;
            if (!opt.default_value.empty()) {
                std::cout << " (default: " << opt.default_value << ")";
            }
            std::cout << "\n";
        }
    }

private:
    int handle_long_option(int argc, const char* const argv[], int i) {
        std::string_view arg = argv[i];
        std::string_view key_part = arg.substr(2);
        size_t eq_pos = key_part.find('=');

        std::string key;
        std::string value;

        if (eq_pos != std::string::npos) {
            key = std::string(key_part.substr(0, eq_pos));
            value = std::string(key_part.substr(eq_pos + 1));
            validate_and_store(key, value);
            return i + 1;
        }

        key = std::string(key_part);
        auto opt = find_option(key);
        if (!opt) {
            throw ConfigurationError("Unknown option: --" + key);
        }

        if (opt->type == Type::Boolean) {
            value = "true";
        } else if (i + 1 < argc && argv[i + 1][0] != '-') {
            value = argv[i + 1];
            validate_and_store(key, value);
            return i + 2;
        } else {
            throw ConfigurationError("Missing value for option: --" + key);
        }

        validate_and_store(key, value);
        return i + 1;
    }

    int handle_short_option(int argc, const char* const argv[], int i) {
        
        std::string_view arg = argv[i];
        std::string_view key_part = arg.substr(1);
        if (key_part.empty()) return i + 1;

        std::string key(key_part.substr(0, 1));
        auto opt = find_option(key);
        if (!opt) {
            throw ConfigurationError("Unknown option: -" + key);
        }

        if (opt->type == Type::Boolean) {
            validate_and_store(opt->long_name, "true");
            return i + 1;
        }

        std::string value;
        if (key_part.length() > 1) {
            value = std::string(key_part.substr(1));
        } else if (i + 1 < argc && argv[i + 1][0] != '-') {
            value = argv[i + 1];
            validate_and_store(opt->long_name, value);
            return i + 2;
        } else {
            throw ConfigurationError("Missing value for option: -" + key);
        }

        validate_and_store(opt->long_name, value);
        return i + 1;
    }

    [[nodiscard]] const Option* find_option(const std::string& key) const {
        auto it = option_map_.find(key);
        return it != option_map_.end() ? it->second : nullptr;
    }

    void validate_and_store(const std::string& key, const std::string& value) {
        auto opt = find_option(key);
        if (!opt) {
            throw ConfigurationError("Unknown option: " + key);
        }
        values_[opt->long_name] = value;
    }

    std::vector<Option> options_;
    std::map<std::string, const Option*> option_map_;
    std::map<std::string, std::string> values_;
};

class AppConfig {
public:
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    static AppConfig& getInstance() {
        static AppConfig instance; 
        return instance;
    }

    void initialize(int argc, char* argv[]) {
        cli_parse_result_ = cli_parser_->parse(argc, argv);
        
        if (!cli_parse_result_.success) {
            throw ConfigurationError(cli_parse_result_.error_message);
        }

        // Load environment file if specified
        auto config_file = cli_parser_->get("config");
        if (config_file) {
            auto ec = env_parser_.load(*config_file);
            if (ec) {
                throw ConfigurationError("Failed to load config file: " + ec.message());
            }
        } else {
            auto ec = env_parser_.load(".env");
            if (ec && ec != std::errc::no_such_file_or_directory) {
                std::cerr << "Warning: Could not load .env file: " << ec.message() << '\n';
            }
        }
    }

      template<typename T>
    T get(const std::string& key) const {
        static_assert(std::is_same_v<T, std::string> || 
                     std::is_same_v<T, bool> ||
                     std::is_same_v<T, int> ||
                     std::is_same_v<T, float>,
                     "Unsupported type");

        if (auto cli_val = cli_parser_->get(key)) { 
            return convert<T>(cli_val->data());
        }

        if (auto env_val = env_parser_.get(key)) {
            return convert<T>(env_val->data());
        }

        for (const auto& opt : cli_parser_options_) {
            if (opt.long_name == key && !opt.default_value.empty()) {
                return convert<T>(opt.default_value);
            }
        }

        throw ConfigurationError("Configuration value not found for key: " + key);
    }

    template<typename T>
    T get(const std::string& key, const T& default_value) const noexcept {
        try {
            return get<T>(key);
        } catch (...) {
            return default_value;
        }
    }

    void print_help() const { cli_parser_->print_help(); }
    bool help_requested() const { return help_requested_; }
    void add_options(std::vector<CLIParser::Option> options) { cli_parser_options_ = options; std::cout<<options[0].long_name<<std::endl; cli_parser_ = new CLIParser(cli_parser_options_); }
private:
    AppConfig() {
        cli_parser_options_ = {
            {"help", "", CLIParser::Type::Boolean, "Show help", "false"}
        };
        cli_parser_ = new CLIParser(cli_parser_options_);
    }

    template<typename T>
    T convert(std::string_view value) const {
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return (value == "true" || value == "1");
        } else if constexpr (std::is_same_v<T, int>) {
            try {
                return std::stoi(std::string(value));
            } catch (...) {
                throw ConfigurationError("Invalid integer value: " + std::string(value));
            }
        } else if constexpr (std::is_same_v<T, float>) {
            try {
                return std::stof(std::string(value));
            } catch (...) {
                throw ConfigurationError("Invalid float value: " + std::string(value));
            }
        }
    }

    CLIParser* cli_parser_ = nullptr;
    EnvParser env_parser_;
    
    std::vector<CLIParser::Option> cli_parser_options_;
    CLIParser::ParseResult cli_parse_result_;
    bool help_requested_ = false;
};

