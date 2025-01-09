// Manager.h
#pragma once

#include <pjsua2.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <atomic>
#include <condition_variable>

#include "jAccount.h"
#include "jCall.h"

class Manager {
public:
    Manager();
    ~Manager();

    void addAccount(const std::string& accountId, const std::string& domain,
        const std::string& username, const std::string& password,
        const std::string& registrarUri, const std::string& agentId = "");
    void removeAccount(const std::string& accountId);
    void makeCall(const std::string& accountId, const std::string& destUri);
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
    std::unordered_map<std::string, std::unique_ptr<jAccount>> m_accounts;
    std::unordered_map<int, std::unique_ptr<jCall>> m_activeCalls;

    TaskQueue m_taskQueue;
    std::unique_ptr<std::thread> m_workerThread;
    std::atomic<bool> m_running{ true };
    std::mutex m_accountsMutex;
    std::mutex m_callsMutex;
};