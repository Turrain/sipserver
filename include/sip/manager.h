// Manager.h
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <pjsua2.hpp>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "agent/agent.h"
#include "sip/account.h"
#include "sip/call.h"

struct RegistrationStatus {
    bool success;
    std::string message;
    int statusCode;
};

struct RegistrationState {
    std::promise<RegistrationStatus> promise;
    bool completed = false;
};

class Manager {
public:
    Manager();
    ~Manager();

    RegistrationStatus addAccount(const std::string &accountId, const std::string &domain,
        const std::string &username, const std::string &password,
        const std::string &registrarUri, const std::string &agentId = "");
    void removeAccount(const std::string &accountId);
    void makeCall(const std::string &accountId, const std::string &destUri);

    void hangupCall(int callId);
    void shutdown();

private:
    class TaskQueue {
    public:
        void enqueue(std::function<void()> task);
        std::function<void()> dequeue();
        void stop();

    private:
        std::queue<std::function<void()>> tasks;
        std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> stopped { false };
    };

    void workerThreadMain();
    void shutdownPjsip();
    void enqueueTask(std::function<void()> task);

    pj::Endpoint m_endpoint;
    std::unordered_map<std::string, std::unique_ptr<Account>> m_accounts;
    std::unordered_map<int, std::unique_ptr<Call>> m_activeCalls;

    TaskQueue m_taskQueue;
    std::unique_ptr<std::thread> m_workerThread;
    std::atomic<bool> m_running { true };
    std::mutex m_accountsMutex;
    std::mutex m_callsMutex;

    AgentManager& m_agentManager = AgentManager::getInstance();
};
