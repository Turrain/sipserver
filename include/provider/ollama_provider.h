#pragma once

#include "provider/provider.h"

class OllamaProvider: public Provider {
private:
    std::string baseUrl;

public:
    std::string getName() const override;
    void configure(const json &config) override;
    std::unique_ptr<Response> handleRequest(const std::unique_ptr<Request> &request) override;
};

class OllamaProviderFactory: public ProviderFactory {
public:
    std::unique_ptr<Provider> createProvider() override;
};