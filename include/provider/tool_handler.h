#pragma once
#include "deps/json.hpp"
#include <functional>
#include <map>
#include <string>
#include <variant>

using json = nlohmann::json;

enum class ArgType {
    String,
    Number,
    Boolean,
    Object,
    Array
};

struct ArgSpec {
    std::string name;
    ArgType type;
    bool required;
    json defaultValue;
};

class ArgumentValidator {
public:
    static bool validateArg(const json &arg, const ArgSpec &spec)
    {
        if (!arg.contains(spec.name)) {
            return !spec.required;
        }

        const auto &value = arg[spec.name];
        switch (spec.type) {
            case ArgType::String:
                return value.is_string();
            case ArgType::Number:
                return value.is_number();
            case ArgType::Boolean:
                return value.is_boolean();
            case ArgType::Object:
                return value.is_object();
            case ArgType::Array:
                return value.is_array();
            default:
                return false;
        }
    }
};

class FunctionHandler {
public:
    using FunctionCallback = std::function<json(const json &)>;

    struct FunctionSpec {
        FunctionCallback callback;
        std::vector<ArgSpec> args;
    };

    void registerFunction(
        const std::string &name,
        FunctionCallback callback,
        const std::vector<ArgSpec> &args)
    {
        functions[name] = FunctionSpec { callback, args };
    }

    json handleFunctionCall(const json &input)
    {
        try {
            if (!input.contains("function")) {
                return makeError("No function specified");
            }

            std::string functionName = input["function"];
            auto it = functions.find(functionName);

            if (it == functions.end()) {
                return makeError("Function not found: " + functionName);
            }

            // Validate arguments
            const auto &spec = it->second;
            for (const auto &argSpec: spec.args) {
                if (!ArgumentValidator::validateArg(input, argSpec)) {
                    return makeError(
                        "Invalid argument: " + argSpec.name + " (required: " + std::string(argSpec.required ? "true" : "false") + ")");
                }
            }

            // Prepare args with defaults
            json args = input;
            for (const auto &argSpec: spec.args) {
                if (!args.contains(argSpec.name) && argSpec.defaultValue != nullptr) {
                    args[argSpec.name] = argSpec.defaultValue;
                }
            }

            return spec.callback(args);

        } catch (const std::exception &e) {
            return makeError(e.what());
        }
    }

private:
    std::map<std::string, FunctionSpec> functions;

    json makeError(const std::string &message)
    {
        return {
            { "status", "error" },
            { "message", message }
        };
    }
};