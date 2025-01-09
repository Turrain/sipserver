#pragma once

#include "provider.h"
#include "request.h"
#include "request_factory.h"
#include <json.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>

using json = nlohmann::json;



class ProviderManager {
private:
    std::map<std::string, std::unique_ptr<Provider>> providers;
    std::map<std::string, std::unique_ptr<RequestFactory>> requestFactories;
    std::map<std::string, std::unique_ptr<ProviderFactory>> providerFactories;
    static ProviderManager *instance;
    static std::mutex mutex;

    ProviderManager();

public:
    static ProviderManager *getInstance();
    void registerProviderFactory(const std::string &name, std::unique_ptr<ProviderFactory> factory);
    void registerRequestFactory(const std::string &name, std::unique_ptr<RequestFactory> factory);
    void loadConfig(const json &configData);
    std::unique_ptr<Response> processRequest(const std::unique_ptr<Request> &request);
    bool hasProvider(const std::string &providerName) const;
    std::unique_ptr<Request> createRequest(const std::string &providerName, Messages messages);
        ProviderManager(const ProviderManager &)
        = delete;
    ProviderManager &operator=(const ProviderManager &) = delete;
};