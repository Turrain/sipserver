#pragma once

#include "json.hpp"
#include <memory>
#include <string>
#include "request.h"
#include "response.h"

using json = nlohmann::json;

class Provider {
public:
    virtual ~Provider() = default;
    virtual std::string getName() const = 0;
    virtual std::unique_ptr<Response> handleRequest(const std::unique_ptr<Request> &request) = 0;
    virtual void configure(const json &config) = 0;
};

class ProviderFactory {
public:
    virtual ~ProviderFactory() = default;
    virtual std::unique_ptr<Provider> createProvider() = 0;
};