#pragma once
#include <sol/sol.hpp>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include <fstream>
#include <deps/json.hpp>

using json = nlohmann::json;

class ProviderManager
{
public:
    struct ProviderConfig
    {
        json parameters;
        json metadata;
    };

    struct RequestResult
    {
        bool success;
        std::string response;
        json metadata;
        std::string error;
    };
   static ProviderManager& getInstance()
    {
        static ProviderManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    ProviderManager(const ProviderManager&) = delete;
    ProviderManager& operator=(const ProviderManager&) = delete;
   

    void register_provider(const std::string &name, sol::function handler)
    {
        providers_[name] = ProviderEntry{
            .handler = std::move(handler),
            .config = load_provider_config(name)};
    }

    RequestResult process_request(
        const std::string &provider_name,
        const std::string &input,
        const json &options = {},
        const json &history = json::array(),
        const json &metadata = json::object())
    {
        if (!providers_.count(provider_name))
        {
            return {false, "", {}, "Provider not registered"};
        }

        auto &provider = providers_.at(provider_name);

        try
        {
            sol::table lua_params = lua_->create_table_with(
                "input", input,
                "config", json_to_lua(provider.config.parameters),
                "options", json_to_lua(options),
                "history", json_to_lua(history),
                "metadata", json_to_lua(metadata));
            std::cout<< options.dump(4) << std::endl;
            sol::protected_function_result result = provider.handler(lua_params);
            if (!result.valid())
            {
                sol::error err = result;
                return {false, "", {}, "Lua error: " + std::string(err.what())};
            }

            auto [success, lua_response, error] = result.get<std::tuple<bool, sol::object, sol::object>>();
            std::string error_str;
            if (error.is<std::string>())
            {
                error_str = error.as<std::string>();
            }
            else if (error.is<sol::nil_t>())
            {
                error_str = "";
            }
            else
            {
                error_str = "Invalid error type returned from Lua";
            }

            RequestResult request_result;
            request_result.success = success;
            request_result.error = error_str;
            std::cout<< error_str << std::endl;
            if (lua_response.is<sol::table>())
            {
                sol::table response_table = lua_response;
                request_result.response = response_table["content"];
                request_result.metadata = lua_to_json(response_table["metadata"]);
                provider.config.metadata.update(lua_to_json(response_table["metadata"]));
                // // Update provider metadata
                // if (response_table["metadata"])
                // {
                //     provider.config.metadata.update(lua_to_json(response_table["metadata"]));
                // }
            }

            return request_result;
        }
        catch (const std::exception &e)
        {
            return {false, "", {}, "Exception: " + std::string(e.what())};
        }
    }

    void set_config_path(const std::filesystem::path &path)
    {
        config_path_ = path;
    }

    void load_providers_from_folder(const std::string &folder_path)
    {
        for (const auto &entry : std::filesystem::directory_iterator(folder_path))
        {
            if (entry.path().extension() == ".lua")
            {
                load_provider(entry.path());
            }
        }
        set_config_path(folder_path);
    }
    void load_provider(const std::filesystem::path &file_path)
    {
        try
        {
            lua_->script_file(file_path.string());
            std::string name = file_path.stem().string();
            sol::function handler = (*lua_)[name];
            if (handler != sol::nil)
            {
                register_provider(name, handler);
            }
            else
            {
               
               // fmt::print(stderr, "Warning: No handler function found for provider '{}' in {}\n", name, file_path.string());
            }
        }
        catch (const sol::error &e)
        {
           // fmt::print(stderr, "Error loading provider from {}: {}\n", file_path.string(), e.what());
        }
    }

private:
 ProviderManager() : lua_(std::make_unique<sol::state>())
    {
        lua_->open_libraries(sol::lib::base, sol::lib::package, sol::lib::table);
        initialize_lua_environment();
    }
    struct ProviderEntry
    {
        sol::function handler;
        ProviderConfig config;
    };

    std::unique_ptr<sol::state> lua_;
    std::unordered_map<std::string, ProviderEntry> providers_;
    std::filesystem::path config_path_ = "./config/";

    void initialize_lua_environment()
    {
        std::cout << "test-------------------------";
        lua_->open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::table,
            sol::lib::string,
            sol::lib::math,
            sol::lib::coroutine,
            sol::lib::debug,
            sol::lib::os);

        std::string package_path = (*lua_)["package"]["path"];
        std::string package_cpath = (*lua_)["package"]["cpath"];

        package_path += ";./lua/?.lua";

        // Add standard Lua and LuaRocks paths
        package_path += ";/usr/local/share/lua/5.4/?.lua";
        package_path += ";/usr/local/share/lua/5.4/?/init.lua";

        // Add C module path
        package_cpath += ";/usr/local/lib/lua/5.4/?.so";

        (*lua_)["package"]["path"] = package_path;
        (*lua_)["package"]["cpath"] = package_cpath;
        std::cout << package_path << std::endl;
        lua_->script(R"(
            -- Prevent insecure OS functions
            os.exit = nil
            os.setlocale = nil
            os.execute = nil
        )");
    }

    ProviderConfig load_provider_config(const std::string &name)
    {
        std::ifstream f(config_path_ / (name + ".json"));
        std::cout << config_path_ / (name + ".json") << std::endl;
        if (!f.is_open())
            return {};

        json config = json::parse(f);
        std::cout << config.dump(2) << std::endl;
        return {
            .parameters = config.value("parameters", json::object()),
            .metadata = config.value("metadata", json::object())};
    }

    sol::object json_to_lua(const json &j)
    {
        if (j.is_null())
            return sol::nil;
        if (j.is_boolean())
            return sol::make_object(lua_->lua_state(), j.get<bool>());
        if (j.is_number())
            return sol::make_object(lua_->lua_state(), j.get<double>());
        if (j.is_string())
            return sol::make_object(lua_->lua_state(), j.get<std::string>());

        if (j.is_array())
        {
            sol::table arr = lua_->create_table();
            for (size_t i = 0; i < j.size(); i++)
            {
                arr[i + 1] = json_to_lua(j[i]);
            }
            return arr;
        }

        if (j.is_object())
        {
            sol::table obj = lua_->create_table();
            for (auto &[key, value] : j.items())
            {
                obj[key] = json_to_lua(value);
            }
            return obj;
        }

        return sol::nil;
    }

    json lua_to_json(const sol::object &obj)
    {
        if (obj == sol::nil)
            return json();
        if (obj.is<bool>())
            return obj.as<bool>();
        if (obj.is<double>())
            return obj.as<double>();
        if (obj.is<std::string>())
            return obj.as<std::string>();

        if (obj.is<sol::table>())
        {
            sol::table table = obj;
            if (table.size() > 0)
            { // Array
                json arr = json::array();
                for (size_t i = 1; i <= table.size(); i++)
                {
                    arr.push_back(lua_to_json(table[i]));
                }
                return arr;
            }
            else
            { // Object
                json obj = json::object();
                table.for_each([&](sol::object key, sol::object value)
                               { obj[key.as<std::string>()] = lua_to_json(value); });
                return obj;
            }
        }

        return json();
    }
};