#pragma once
#include <deps/json.hpp>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <optional>

using json = nlohmann::json;

struct ProviderConfig {
    std::string name;
    std::string script_path;
    json overrides;
    bool enabled;
};

class Configuration {
public:
    explicit Configuration(const std::string& config_path) {
        lua_.open_libraries(sol::lib::base, sol::lib::package,
                          sol::lib::table, sol::lib::string);
        lua_.script_file(config_path);
        parse_config();
    }

    const std::unordered_map<std::string, ProviderConfig>& providers() const {
        return providers_;
    }

    std::string default_provider() const {
        return default_provider_;
    }

    void update_provider_config(const std::string& name, const json& new_config) {
        if (providers_.find(name) != providers_.end()) {
            providers_[name].overrides.update(new_config);
        }
    }

    void set_default_provider(const std::string& name) {
        if (providers_.find(name) != providers_.end()) {
            default_provider_ = name;
        }
    }

    void enable_provider(const std::string& name, bool enabled = true) {
        if (providers_.find(name) != providers_.end()) {
            providers_[name].enabled = enabled;
        }
    }

    json get_provider_config(const std::string& name) const {
        if (providers_.find(name) != providers_.end()) {
            return providers_.at(name).overrides;
        }
        return json();
    }

private:
    void parse_config() {
        sol::table config = lua_["cfg"];
        default_provider_ = config.get_or<std::string>("default_provider", "");

        sol::table providers = config["providers"];
        providers.for_each([this](sol::object key, sol::object value) {
            std::string name = key.as<std::string>();
            sol::table provider_data = value.as<sol::table>();

            auto lua_state = lua_.lua_state();
            sol::table default_table(lua_state);
            sol::table config_overrides = provider_data.get_or<sol::table>("config_overrides", default_table);

            ProviderConfig pc{
                .name = name,
                .script_path = provider_data["script_path"],
                .overrides = sol_to_json(config_overrides),
                .enabled = provider_data.get_or("enabled", true)
            };

            providers_.emplace(name, std::move(pc));
        });
    }

    json sol_to_json(const sol::table& table) const {
        json j;
        table.for_each([&j, this](sol::object key, sol::object value) {
            std::string k = key.as<std::string>();
            if (value.is<std::string>()) {
                j[k] = value.as<std::string>();
            }
            else if (value.is<double>()) {
                j[k] = value.as<double>();
            }
            else if (value.is<bool>()) {
                j[k] = value.as<bool>();
            }
            else if (value.is<sol::table>()) {
                j[k] = sol_to_json(value.as<sol::table>());
            }
        });
        return j;
    }

    sol::state lua_;
    std::unordered_map<std::string, ProviderConfig> providers_;
    std::string default_provider_;
};