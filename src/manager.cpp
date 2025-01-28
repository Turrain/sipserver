// Manager.cpp
#include "sip/manager.h"
#include "agent/agent.h"
#include "utils/logger.h"
#include <iostream>
#include <memory>

Manager::Manager(std::shared_ptr<AgentManager> manager) :
    m_agentManager(manager)
{
    try {

        // Initialize PJSIP endpoint
        m_endpoint.libCreate();

        pj::EpConfig epConfig;
        epConfig.logConfig.level = 4;
        m_endpoint.libInit(epConfig);

        // Create UDP transport
        pj::TransportConfig transportConfig;
        transportConfig.port = 0;
        m_endpoint.transportCreate(PJSIP_TRANSPORT_UDP, transportConfig);

        // Disable audio device
        m_endpoint.audDevManager().setNullDev();

        // Start library
        m_endpoint.libStart();
        LOG_DEBUG << "PJSIP initialized";
        // Start worker thread
        m_workerThread = std::make_unique<std::thread>(&Manager::workerThreadMain, this);
    } catch (pj::Error &err) {
        std::cerr << "PJSIP Initialization Error: " << err.info() << std::endl;
        throw;
    }
}

Manager::~Manager() { shutdown(); }

RegistrationStatus Manager::addAccount(const std::string &accountId,
    const std::string &domain,
    const std::string &username,
    const std::string &password,
    const std::string &registrarUri,
    const std::string &agentId)
{
    std::promise<RegistrationStatus> registrationPromise;
    auto registrationFuture = registrationPromise.get_future();
    enqueueTask([this, accountId, domain, username, password, registrarUri, agentId, &registrationPromise]() {
        try {
            std::lock_guard<std::mutex> lock(m_accountsMutex);

            if (m_accounts.find(accountId) != m_accounts.end()) {
                throw std::invalid_argument("Account already exists: " + accountId);
            }

            pj::AccountConfig accountConfig;
            accountConfig.idUri = "sip:" + username + "@" + domain;
            accountConfig.regConfig.registrarUri = registrarUri;
            accountConfig.regConfig.timeoutSec = 20;
            accountConfig.regConfig.retryIntervalSec = 2;

            pj::AuthCredInfo credInfo("digest", "*", username, 0, password);
            accountConfig.sipConfig.authCreds.push_back(credInfo);

            auto account = std::make_unique<Account>(m_agentManager);

            account->registerRegStateCallback([&registrationPromise](auto state, auto status) {
                if (status == PJSIP_SC_OK) {
                    registrationPromise.set_value({ true,
                        "Registration successful",
                        status });
                }
            });

            account->create(accountConfig);
            if (!agentId.empty()) {
                account->setAgent(agentId);
            }

            m_accounts[accountId] = std::move(account);

        } catch (const pj::Error &err) {
            registrationPromise.set_value({ false,
                "PJSIP Error: " + std::string(err.info()),
                500 });
        } catch (const std::exception &e) {
            registrationPromise.set_value({ false,
                "Error: " + std::string(e.what()),
                500 });
        }
    });
    auto status = registrationFuture.wait_for(std::chrono::seconds(20));
    if (status == std::future_status::timeout) {
        return { false, "Registration timeout", 408 };
    }
    return registrationFuture.get();
}

void Manager::removeAccount(const std::string &accountId)
{
    enqueueTask([this, accountId]() {
        try {
            std::lock_guard<std::mutex> lock(m_accountsMutex);

            auto it = m_accounts.find(accountId);
            if (it != m_accounts.end()) {
                it->second->shutdown();
                m_accounts.erase(it);

            } else {
            }
        } catch (const pj::Error &err) {
        }
    });
}

void Manager::makeCall(const std::string &accountId, const std::string &destUri)
{
    enqueueTask([this, accountId, destUri]() {
        try {
            std::lock_guard<std::mutex> lock(m_accountsMutex);
            auto t = m_accounts.find(accountId);
            auto accountIt = m_accounts.find(accountId);
            if (accountIt == m_accounts.end()) {
                throw std::invalid_argument("Account not found: " + accountId);
            }

            pj::CallOpParam callOpParam;
            auto call = std::make_unique<Call>(*accountIt->second);
            call->makeCall(destUri, callOpParam);

            {
                std::lock_guard<std::mutex> callsLock(m_callsMutex);
                m_activeCalls[call->getId()] = std::move(call);
            }
        } catch (const pj::Error &err) {
            std::cerr << "Error making call: " << err.info() << std::endl;
        }
    });
}

void Manager::hangupCall(int callId)
{
    enqueueTask([this, callId]() {
        try {
            std::lock_guard<std::mutex> lock(m_callsMutex);

            auto it = m_activeCalls.find(callId);
            if (it != m_activeCalls.end()) {
                pj::CallOpParam callOpParam;
                callOpParam.statusCode = PJSIP_SC_DECLINE;
                it->second->hangup(callOpParam);
                m_activeCalls.erase(it);

            } else {
            }
        } catch (const pj::Error &err) {
        }
    });
}

void Manager::shutdown()
{
    m_running = false;
    m_taskQueue.stop();

    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
    }
}

void Manager::TaskQueue::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopped)
            return;
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

std::function<void()> Manager::TaskQueue::dequeue()
{
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [this] { return !tasks.empty() || stopped; });

    if (stopped && tasks.empty()) {
        return nullptr;
    }

    auto task = std::move(tasks.front());
    tasks.pop();
    return task;
}

void Manager::TaskQueue::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopped = true;
    }
    condition.notify_all();
}

void Manager::workerThreadMain()
{
    pj_thread_desc threadDesc;
    pj_thread_t *thread = nullptr;

    // Register this thread with PJSIP
    if (pj_thread_register("WorkerThread", threadDesc, &thread) != PJ_SUCCESS) {
        std::cerr << "Failed to register worker thread" << std::endl;
        return;
    }

    while (m_running) {
        try {
            auto task = m_taskQueue.dequeue();
            if (task)
                task();
        } catch (const std::exception &e) {
            std::cerr << "Worker Thread Error: " << e.what() << std::endl;
        }
    }
    shutdownPjsip();
}

void Manager::shutdownPjsip()
{
    // Hangup all active calls
    {
        std::lock_guard<std::mutex> lock(m_callsMutex);
        for (auto &[id, call]: m_activeCalls) {
            try {
                pj::CallOpParam callOpParam;
                callOpParam.statusCode = PJSIP_SC_DECLINE;
                call->hangup(callOpParam);
            } catch (...) {
                std::cerr << "Error hanging up call: " << id << std::endl;
            }
        }
        m_activeCalls.clear();
    }

    // Remove all accounts
    {
        std::lock_guard<std::mutex> lock(m_accountsMutex);
        m_accounts.clear();
    }

    // Destroy PJSIP library
    m_endpoint.libDestroy();
}

void Manager::enqueueTask(std::function<void()> task)
{
    if (m_running) {
        m_taskQueue.enqueue(std::move(task));
    } else {
        throw std::runtime_error("Manager is shutting down");
    }
}
