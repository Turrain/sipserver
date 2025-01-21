#include "provider/provider_manager.h"
#include "utils/logger.h"
#include <iostream>

ProviderManager *ProviderManager::instance = nullptr;
std::mutex ProviderManager::mutex;

ProviderManager::ProviderManager()
{
    Configuration config("lua/config.lua");
    luaManager = std::make_unique<LuaProviderManager>(config);
}

ProviderManager *ProviderManager::getInstance()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (instance == nullptr) {
        instance = new ProviderManager();
    }
    return instance;
}

ProviderResponse ProviderManager::processRequest(const std::string &providerName, const std::string &input)
{
    if (!luaManager) {
        return {"Error: LuaProviderManager not initialized", {}};
    }
    return luaManager->call_provider(providerName, input);
}

bool ProviderManager::hasProvider(const std::string &providerName) const
{
    // TODO: Add method to LuaProviderManager to check if provider exists
    try {
        if (!luaManager) {
            return false;
        }
        // Try to call the provider with an empty input to check if it exists
        luaManager->call_provider(providerName, "");
        return true;
    } catch (...) {
        return false;
    }
}

void ProviderManager::updateProviderConfig(const std::string &name, const json &newConfig)
{
    if (luaManager) {
        luaManager->update_provider_config(name, newConfig);
    }
}
