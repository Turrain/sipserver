#pragma once
#include "request.h"

class RequestFactory {
public:
    virtual ~RequestFactory() = default;
    virtual std::unique_ptr<Request> createRequest(const std::string &message) = 0;
};

class OllamaRequestFactory: public RequestFactory {
public:
    std::unique_ptr<Request> createRequest(const std::string &message) override
    {
        return std::make_unique<OllamaRequest>(message, ""); // Model will be set later
    }
};

class GroqRequestFactory: public RequestFactory {
public:
    std::unique_ptr<Request> createRequest(const std::string &message) override
    {
        return std::make_unique<GroqRequest2>(message);
    }
};