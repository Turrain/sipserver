#include "provider/provider_manager.h"
#include "utils/logger.h"
#include <iostream>

// Static member definitions
ProviderManager* ProviderManager::instance = nullptr;
std::mutex ProviderManager::mutex;

void ProviderManager::initialize(core::ScopedConfiguration config) {
    Logger::setMinLevel(Level::Debug);  // Enable debug logging
    try {
        // Create the manager first
        luaManager = std::make_unique<LuaProviderManager>();
        
        // Now load the initial configuration
        auto providers = config.get<nlohmann::json>("");
        LOG_CRITICAL << "PROVIDER"<< config.getData().dump(4);
        luaManager->configure(providers);
        
        // // Add observer for future changes
        // config.observe("providers", [this](const nlohmann::json& config_data) {
        //     if (luaManager) {
        //         luaManager->configure(config_data);
        //     }
        // });
        
        LOG_DEBUG << "Provider manager initialized successfully";
    } catch (const std::exception &e) {
        LOG_ERROR << "Failed to initialize provider manager: " << e.what();
        luaManager.reset();
    }
}
