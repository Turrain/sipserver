#pragma once
#include "common/message.h"
#include "core/configuration.h"
#include "utils/logger.h"
#include <sol/sol.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <any>
#include <deps/json.hpp>
#include <httplib.h>
#include <memory>
#include <unordered_map>
#include <vector>

using Messages = std::vector<Message>;
using json = nlohmann::json;

struct ProviderResponse {
    std::string content;
    std::unordered_map<std::string, std::any> metadata;

    bool operator==(const ProviderResponse &other) const
    {
        return content == other.content;
    }

    bool operator!=(const ProviderResponse &other) const
    {
        return !(*this == other);
    }
};
inline sol::object json_to_sol(lua_State *L, const json &j)
{
    sol::state_view lua(L);
    if (j.is_object()) {
        sol::table t = lua.create_table();
        for (auto &[key, val]: j.items()) {
            t[key] = json_to_sol(L, val);
        }
        return t;
    } else if (j.is_array()) {
        sol::table t = lua.create_table();
        size_t index = 1;
        for (const auto &elem: j) {
            t[index++] = json_to_sol(L, elem);
        }
        return t;
    } else if (j.is_string()) {
        return sol::make_object(L, j.get<std::string>());
    } else if (j.is_number()) {
        return sol::make_object(L, j.get<double>());
    } else if (j.is_boolean()) {
        return sol::make_object(L, j.get<bool>());
    } else if (j.is_null()) {
        return sol::make_object(L, sol::lua_nil);
    }
    return sol::make_object(L, sol::lua_nil);
}

namespace sol {
namespace stack {

inline int push(lua_State *L, const json &j)
{
    sol::state_view lua(L);
    sol::object obj = json_to_sol(lua, j);
    return stack::push(L, obj);
}

} // namespace stack
} // namespace sol

class LuaProvider {
public:
    LuaProvider(sol::table config, sol::function request_fn) :
        config_(std::move(config)),
        request_fn_(std::move(request_fn)) { }

    ProviderResponse send_request(const std::string &input,
        nlohmann::json options = {})
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::package,
            sol::lib::table, sol::lib::string);

        lua["print"] = [](const std::string &msg) {
            LOG_DEBUG << "[Lua] " << msg;
        };

        LOG_DEBUG << "Lua function called";
        LOG_WARNING << options.dump(2);

        try {
            // Call the Lua function
            sol::protected_function_result result = request_fn_(config_, input, options);

            // Check if the result is valid
            if (!result.valid()) {
                sol::error err = result;
                return { "Lua Error: " + std::string(err.what()), {} };
            }

            // Safely extract the result as a table
            sol::optional<sol::table> res_table = result;
            if (!res_table) {
                return { "Invalid response format (not a table)", {} };
            }

            // Extract content and metadata
            sol::optional<std::string> content = (*res_table)["content"];
            sol::optional<sol::table> metadata = (*res_table)["metadata"];

            if (!content) {
                return { "Invalid response format (missing 'content')", {} };
            }

            // Build the response
            ProviderResponse response;
            response.content = *content;

            if (metadata) {
                metadata->for_each([&](sol::object key, sol::object value) {
                    std::string k = key.as<std::string>();
                    if (value.is<std::string>()) {
                        response.metadata[k] = value.as<std::string>();
                    } else if (value.is<double>()) {
                        response.metadata[k] = value.as<double>();
                    } else if (value.is<bool>()) {
                        response.metadata[k] = value.as<bool>();
                    }
                });
            }

            return response;
        } catch (const sol::error &e) {
            return { "Lua Error: " + std::string(e.what()), {} };
        }
    }

    void update_config(const json &new_config)
    {
        for (const auto &[key, value]: new_config.items()) {
            if (value.is_string()) {
                config_[key] = value.get<std::string>();
            } else if (value.is_number()) {
                config_[key] = value.get<double>();
            } else if (value.is_boolean()) {
                config_[key] = value.get<bool>();
            }
        }
    }

private:
    sol::table create_lua_table(sol::state &lua,
        const std::unordered_map<std::string, std::any> &options)
    {
        sol::table tbl = lua.create_table();
        for (const auto &[key, value]: options) {
            if (auto str = std::any_cast<std::string>(&value)) {
                tbl[key] = *str;
            } else if (auto num = std::any_cast<double>(&value)) {
                tbl[key] = *num;
            } else if (auto flag = std::any_cast<bool>(&value)) {
                tbl[key] = *flag;
            }
        }
        return tbl;
    }

    ProviderResponse parse_response(const sol::object &result)
    {
        if (result.get_type() == sol::type::table) {
            sol::table res_table = result;
            ProviderResponse response;
            response.content = res_table["content"];

            sol::optional<sol::table> meta = res_table["metadata"];
            if (meta) {
                meta->for_each([&](sol::object key, sol::object value) {
                    std::string k = key.as<std::string>();
                    if (value.is<std::string>()) {
                        response.metadata[k] = value.as<std::string>();
                    } else if (value.is<double>()) {
                        response.metadata[k] = value.as<double>();
                    } else if (value.is<bool>()) {
                        response.metadata[k] = value.as<bool>();
                    }
                });
            }
            return response;
        }
        return { "Invalid response format", {} };
    }

    sol::table config_;
    sol::function request_fn_;
};

class LuaProviderManager {
public:
    LuaProviderManager()
    {
        initialize_lua_bindings();
    }

    void configure(const nlohmann::json &providers_config)
    {
        providers_.clear(); // Clear existing providers
        try {
            for (const auto &[name, provider]: providers_config.items()) {
                if (!provider["enabled"].get<bool>())
                    continue;
                LOG_DEBUG << "Loading provider: " << name;
                try {
                    // Create provider config table in the global Lua state
                    sol::table provider_config = lua_.create_table();

                    // Load provider's config overrides
                    if (provider.contains("config")) {
                        const auto &overrides = provider["config"];
                        for (const auto &[key, value]: overrides.items()) {
                            if (value.is_string()) {
                                provider_config[key] = value.get<std::string>();
                            } else if (value.is_number()) {
                                provider_config[key] = value.get<double>();
                            } else if (value.is_boolean()) {
                                provider_config[key] = value.get<bool>();
                            }
                        }
                    }

                    // Create a function to get the config
                    lua_["get_provider_config"] = [provider_config]() { return provider_config; };

                    // Load and execute the provider script
                    lua_.script_file(provider["script_path"].get<std::string>());
                    LOG_DEBUG << "Loaded provider: " << name;

                    // Clean up temporary config
                    lua_["temp_config"] = sol::nil;
                } catch (const sol::error &e) {
                    LOG_ERROR << "Error loading provider " << name << ": " << e.what();
                }
            }
        } catch (const std::exception &e) {
            LOG_ERROR << "Error loading providers: " << e.what();
        }
    }

    ProviderResponse call_provider(const std::string &name,
        const std::string &input,
        nlohmann::json options = {})
    {
        LOG_DEBUG << "Calling provider: " << name;
        auto it = providers_.find(name);
        return it != providers_.end() ? it->second->send_request(input, options) : ProviderResponse { "Provider not found", {} };
    }

    void register_provider(const std::string &name, sol::table config, sol::function request_fn)
    {
        providers_[name] = std::make_unique<LuaProvider>(std::move(config), std::move(request_fn));
    }

    bool has_provider(const std::string &name) const
    {
        return providers_.find(name) != providers_.end();
    }

private:
    void initialize_lua_bindings()
    {
        lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math);

        lua_["print"] = [](const std::string &msg) {
            LOG_DEBUG << "[Lua] " << msg;
        };

        sol::table package = lua_["package"];
        std::string current_path = package["path"];
        package["path"] = current_path + ";./lua/?.lua";

        lua_.new_usertype<LuaProviderManager>(
            "LuaProviderManager",
            "register_provider", &LuaProviderManager::register_provider);

        lua_["http_post"] = [](const std::string &url,
                                const std::string &path,
                                sol::table headers,
                                const std::string &body) {
            auto is_https = [](const std::string &url) {
                return url.substr(0, 8) == "https://";
            };
            std::string hostname = url;
            if (hostname.substr(0, 7) == "http://") {
                hostname = hostname.substr(7);
            } else if (hostname.substr(0, 8) == "https://") {
                hostname = hostname.substr(8);
            }
            LOG_DEBUG << "Hostname: " << hostname;

            httplib::Headers hdrs;
            headers.for_each([&hdrs](sol::object key, sol::object value) {
                hdrs.emplace(key.as<std::string>(), value.as<std::string>());
            });
            LOG_DEBUG << "Headers: " << hdrs.size();
            LOG_DEBUG << "Body: " << body;
            if (is_https(url)) {
                httplib::SSLClient cli(hostname, 443);
                cli.enable_server_certificate_verification(true);
                auto res = cli.Post(path, hdrs, body, "application/json");
                if (res) {
                    LOG_DEBUG << "HTTP Request Successful: " << res->body;
                    return res->body;
                } else {
                    LOG_ERROR << "HTTP Request Failed: " << res.error();
                    return std::string("HTTP request failed");
                }
            } else {
                httplib::Client cli(hostname);
                auto res = cli.Post(path, hdrs, body, "application/json");
                if (res) {
                    LOG_DEBUG << "HTTP Request Successful: " << res->body;
                    return res->body;
                } else {
                    LOG_ERROR << "HTTP Request Failed: " << res.error();
                    return std::string("HTTP request failed");
                }
            }
        };

        lua_["manager"] = this;
    }

    sol::state lua_;
    std::unordered_map<std::string, std::unique_ptr<LuaProvider>> providers_;
};
