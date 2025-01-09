#pragma once
#include "provider/request.h"
#include <vector>

class RequestFactory {
public:
    virtual ~RequestFactory() = default;
    virtual std::unique_ptr<Request> createRequest(std::vector<Message> message) = 0;
};

class OllamaRequestFactory: public RequestFactory {
public:
    std::unique_ptr<Request> createRequest(std::vector<Message> message) override
    {
        return std::make_unique<OllamaRequest>(message); // Model will be set later
    }
};

class GroqRequestFactory: public RequestFactory {
public:
    std::unique_ptr<Request> createRequest(std::vector<Message> message) override
    {
        return std::make_unique<GroqRequest2>(message);
    }
};