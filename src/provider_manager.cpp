#include "provider/provider_manager.h"
#include "provider/groq_provider.h"
#include "provider/ollama_provider.h"
#include "provider/request.h"
#include "provider/request_factory.h"
#include "utils/logger.h"
#include <iostream>

ProviderManager *ProviderManager::instance = nullptr;
std::mutex ProviderManager::mutex;

ProviderManager::ProviderManager()
{
    registerProviderFactory("Ollama", std::make_unique<OllamaProviderFactory>());
    registerProviderFactory("Groq", std::make_unique<GroqProviderFactory>());
    registerRequestFactory("Ollama", std::make_unique<OllamaRequestFactory>());
    registerRequestFactory("Groq", std::make_unique<GroqRequestFactory>());
}

ProviderManager *ProviderManager::getInstance()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (instance == nullptr) {
        instance = new ProviderManager();
    }
    return instance;
}

void ProviderManager::registerProviderFactory(const std::string &name, std::unique_ptr<ProviderFactory> factory)
{
    providerFactories[name] = std::move(factory);
}

void ProviderManager::registerRequestFactory(const std::string &name, std::unique_ptr<RequestFactory> factory)
{
    requestFactories[name] = std::move(factory);
}

void ProviderManager::loadConfig(const json &configData)
{
    if (configData.contains("providers") && configData["providers"].is_object()) {
        for (auto &[providerName, providerConfig]: configData["providers"].items()) {
            if (providerFactories.count(providerName)) {
                auto provider = providerFactories[providerName]->createProvider();
                provider->configure(providerConfig);
                providers[providerName] = std::move(provider);
            } else {
                std::cerr << "Error: Unknown provider type '" << providerName << "' in config." << std::endl;
            }
        }
    }
}

std::unique_ptr<Response> ProviderManager::processRequest(const std::unique_ptr<Request> &request)
{
    std::string providerName = request->getProviderName();
    if (providers.count(providerName)) {
        return providers[providerName]->handleRequest(request);
    } else {
        std::cerr << "Error: Provider '" << providerName << "' not found." << std::endl;
        return nullptr;
    }
}

bool ProviderManager::hasProvider(const std::string &providerName) const
{
    return providers.count(providerName) > 0;
}

std::unique_ptr<Request> ProviderManager::createRequest(const std::string &providerName, Messages message)
{
    if (requestFactories.count(providerName)) {
        return requestFactories[providerName]->createRequest(message);
    }
    LOG_ERROR << "Request factory not found for provider: " << providerName;
    return nullptr;
}