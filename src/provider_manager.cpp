#include "provider/provider_manager.h"
#include "utils/logger.h"
#include <iostream>

// Static member definitions
ProviderManager* ProviderManager::instance = nullptr;
std::mutex ProviderManager::mutex;

void ProviderManager::initialize(Configuration &config) {
    try {
        luaManager = std::make_unique<LuaProviderManager>(config);
        LOG_DEBUG << "Provider manager initialized successfully";
    } catch (const std::exception &e) {
        LOG_ERROR << "Failed to initialize provider manager: " << e.what();
        luaManager.reset();
    }
}
