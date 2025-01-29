#pragma once

#include "core/configuration.h"
#include "provider/lua_provider.h"
#include <memory>
#include <string>
#include <mutex>

class ProviderManager {
private:
    std::unique_ptr<LuaProviderManager> luaManager;
    static ProviderManager* instance;
    static std::mutex mutex;

    ProviderManager() = default;

public:
    static ProviderManager* getInstance() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!instance) {
            instance = new ProviderManager();
        }
        return instance;
    }

    ProviderResponse processRequest(const std::string& providerName, const std::string& input, nlohmann::json options = {}, Messages history = {}) {
        return luaManager ? luaManager->call_provider(providerName, input, options, history) 
                        : ProviderResponse{"Provider manager not initialized", {}};
    }

    bool hasProvider(const std::string& providerName) const {
        return luaManager && luaManager->has_provider(providerName);
    }

    void initialize(core::ScopedConfiguration config);
    
    ProviderManager(const ProviderManager&) = delete;
    ProviderManager& operator=(const ProviderManager&) = delete;
};
