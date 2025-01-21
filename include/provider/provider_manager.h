#pragma once

#include "provider/lua_provider.h"
#include <memory>
#include <string>

class ProviderManager {
private:
    std::unique_ptr<LuaProviderManager> luaManager;
    static ProviderManager *instance;
    static std::mutex mutex;

    ProviderManager();

public:
    static ProviderManager *getInstance();
    ProviderResponse processRequest(const std::string &providerName, const std::string &input);
    bool hasProvider(const std::string &providerName) const;
    void updateProviderConfig(const std::string &name, const json &newConfig);
    
    ProviderManager(const ProviderManager &) = delete;
    ProviderManager &operator=(const ProviderManager &) = delete;
};
