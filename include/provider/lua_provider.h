#pragma once
#include "lua_config.h"
#include <sol/sol.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <any>
#include <httplib.h>
#include <memory>
#include <unordered_map>

struct ProviderResponse {
    std::string content;
    std::unordered_map<std::string, std::any> metadata;
};

class LuaProvider {
public:
    LuaProvider(sol::table config, sol::function request_fn) :
        config_(std::move(config)),
        request_fn_(std::move(request_fn)) { }

    ProviderResponse send_request(const std::string &input,
        const std::unordered_map<std::string, std::any> &options = {})
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::package,
            sol::lib::table, sol::lib::string);

        sol::table lua_options = create_lua_table(lua, options);

        try {
            // Call the Lua function
            sol::protected_function_result result = request_fn_(config_, input, lua_options);

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
    explicit LuaProviderManager(Configuration &config) :
        config_(config)
    {
        initialize_lua_bindings();
        load_providers();
    }

    ProviderResponse call_provider(const std::string &name,
        const std::string &input,
        const std::unordered_map<std::string, std::any> &options = {})
    {
        auto it = providers_.find(name);
        return it != providers_.end() ? it->second->send_request(input, options) : ProviderResponse { "Provider not found", {} };
    }

    void update_provider_config(const std::string &name, const json &new_config)
    {
        if (providers_.find(name) != providers_.end()) {
            providers_[name]->update_config(new_config);
            config_.update_provider_config(name, new_config);
        }
    }

    void register_provider(const std::string &name, sol::table config, sol::function request_fn)
    {
        providers_[name] = std::make_unique<LuaProvider>(std::move(config), std::move(request_fn));
    }

private:
    void initialize_lua_bindings()
    {
        lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math);
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
            std::cout << "Hostname: " << hostname << std::endl;
            httplib::Headers hdrs;
            headers.for_each([&hdrs](sol::object key, sol::object value) {
                hdrs.emplace(key.as<std::string>(), value.as<std::string>());
            });

            if (is_https(url)) {
                httplib::SSLClient cli(hostname, 443);
                cli.enable_server_certificate_verification(true);
                auto res = cli.Post(path, hdrs, body, "application/json");
                if (res) {
                    return res->body;
                } else {
                    LOG_ERROR << "HTTP Request Failed: " << res.error();

                    return std::string("HTTP request failed");
                }
            } else {
                httplib::Client cli(hostname);
                auto res = cli.Post(path, hdrs, body, "application/json");
                if (res) {
                    return res->body;
                } else {
                    LOG_ERROR << "HTTP Request Failed: " << res.error();

                    return std::string("HTTP request failed");
                }
            }
        };

        lua_["manager"] = this;
    }

    void load_providers()
    {
        for (const auto &[name, pc]: config_.providers()) {
            if (!pc.enabled)
                continue;

            try {
                lua_.script_file(pc.script_path);
                LOG_DEBUG << "Loaded provider: " << name;

            } catch (const sol::error &e) {
                LOG_ERROR << "Error loading provider " << name << ": " << e.what();
            }
        }
    }

    sol::state lua_;
    std::unordered_map<std::string, std::unique_ptr<LuaProvider>> providers_;
    Configuration &config_;
};