#pragma once

#include "provider.h"

class GroqProvider: public Provider {
private:
    std::string apiKey;
    std::string baseUrl;

public:
    std::string getName() const override;
    void configure(const json &config) override;
    std::unique_ptr<Response> handleRequest(const std::unique_ptr<Request> &request) override;
};

class GroqProviderFactory: public ProviderFactory {
public:
    std::unique_ptr<Provider> createProvider() override;
};