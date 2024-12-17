#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "httplib.h"
#include "jmanager.h"
#include "json.hpp"
#include "llm_manager.h"

// TODO: In

class PJSIPController {
private:
    Manager &m_sipManager;
    httplib::Server m_server;

public:
    PJSIPController(Manager &sipManager) :
        m_sipManager(sipManager)
    {
        setupRoutes();
    }

    void run(uint16_t port = 18080)
    {
        std::cout << "Starting PJSIP REST API on port " << port << "..."
                  << std::endl;
        m_server.listen("0.0.0.0", port);
    }

private:
    void setupRoutes()
    {
        // Accounts Add
        m_server.Post("/accounts/add", [this](const httplib::Request &req, httplib::Response &res) {
            // Parse JSON
            nlohmann::json x;
            try {
                x = nlohmann::json::parse(req.body);
            } catch (...) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }

            if (!x.contains("accountId") || !x.contains("domain") || !x.contains("username") || !x.contains("password") || !x.contains("registrarUri")) {
                res.status = 400;
                res.set_content("Missing required fields", "text/plain");
                return;
            }

            std::string accountId = x["accountId"].get<std::string>();
            std::string domain = x["domain"].get<std::string>();
            std::string username = x["username"].get<std::string>();
            std::string password = x["password"].get<std::string>();
            std::string registrar = x["registrarUri"].get<std::string>();

            // Initiate the action on Manager
            m_sipManager.addAccount(accountId, domain, username, password, registrar);

            // Block and wait for the manager to complete and set a status
            // code/response Assuming the manager sets some flag or triggers
            // completion For demo, we just loop until we hypothetically have a status
            // set
            while (res.status == 0) {
                std::this_thread::yield();
            }
        });

        // Make Call
        m_server.Post("/calls/make",
            [this](const httplib::Request &req, httplib::Response &res) {
                nlohmann::json x;
                try {
                    x = nlohmann::json::parse(req.body);
                } catch (...) {
                    res.status = 400;
                    res.set_content("Invalid JSON", "text/plain");
                    return;
                }

                if (!x.contains("accountId") || !x.contains("destUri")) {
                    res.status = 400;
                    res.set_content("Missing required fields", "text/plain");
                    return;
                }

                std::string accountId = x["accountId"].get<std::string>();
                std::string destUri = x["destUri"].get<std::string>();

                m_sipManager.makeCall(accountId, destUri);

                // Block until done
                while (res.status == 0) {
                    std::this_thread::yield();
                }
            });

        // Hangup Call
        m_server.Post("/calls/hangup",
            [this](const httplib::Request &req, httplib::Response &res) {
                nlohmann::json x;
                try {
                    x = nlohmann::json::parse(req.body);
                } catch (...) {
                    res.status = 400;
                    res.set_content("Invalid JSON", "text/plain");
                    return;
                }

                if (!x.contains("callId")) {
                    res.status = 400;
                    res.set_content("Missing callId field", "text/plain");
                    return;
                }

                int callId = x["callId"].get<int>();
                m_sipManager.hangupCall(callId);

                // Block until done
                while (res.status == 0) {
                    std::this_thread::yield();
                }
            });

        // Remove Account
        m_server.Delete("/accounts/remove", [this](const httplib::Request &req, httplib::Response &res) {
            nlohmann::json x;
            try {
                x = nlohmann::json::parse(req.body);
            } catch (...) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
                return;
            }

            if (!x.contains("accountId")) {
                res.status = 400;
                res.set_content("Missing accountId field", "text/plain");
                return;
            }

            std::string accountId = x["accountId"].get<std::string>();
            m_sipManager.removeAccount(accountId);

            // Block until done
            while (res.status == 0) {
                std::this_thread::yield();
            }
        });
    }
};
std::string virtualAssistantInstruction =
    R"(Роль: Виртуальный оператор первой линии поддержки клиентов Береке банка

Задача: Оперативно решать вопросы клиентов, связанные с блокировкой карты, непонятными списаниями, потерей карты и консультацией по ближайшему отделению банка.

Ключевые принципы:
1. Отвечать вежливо, спокойно, чётко и профессионально
2. Общаться только на темы, связанные с Береке банком
3. Не флиртовать, не использовать смайлики и другие символы
4. Не фантазировать и не придумывать ничего

Алгоритм разговора:

Шаг 1: Приветствие и выявление проблемы
'Добро пожаловать в Береке Банк! Я ваш виртуальный консультант Сабина. Чем могу Вам помочь?'

Если у клиента списываются деньги:
'Я понимаю вашу ситуацию и помогу решить вопрос. У вас есть доступ к мобильному приложению банка?'

Если клиент говорит про потерю карты:
'Понял вас, предлагаю заблокировать карту. У вас есть доступ к мобильному приложению банка?'

Шаг 2: Если у клиента есть мобильное приложение
Ответ клиента: Да
'Отлично! Тогда сделайте следующее:
1. Выберите вашу карту в приложении.
2. Перейдите в раздел 'Действия' и выберите 'Заблокировать'.
3. Нажмите 'Подтвердить действие'.'

Больше ничего не говори, пока клиент что-нибудь не ответит

Если клиент говорит, что не получается/не хочет блокировать самостоятельно:
Перейдите к Шагу 3.

Если клиент говорит, что все получилось:
'Хорошо! Для перевыпуска карты Вы можете обратиться в ближайшее отделение банка.'

Шаг 3: Если у клиента нет приложения / он не может заблокировать карту
Ответ клиента: Нет приложения / не могу заблокировать / не получается / не понимаю
'Понял вас. Я переведу вас на специалиста.'

После получения данных:
'Спасибо, (Имя Отчество). Соединяю вас со специалистом.'
тут разговор заканчивается

Шаг 4: Дополнительные вопросы клиента

1. Если клиент спрашивает про ближайшее отделение:
'Адрес центрального отделения город Алматы, проспект Аль-Фараби, тринадцать.'

2. Если клиент говорит на тему, не связанную с банком береке:
'Я виртуальный консультант Береке банка, и могу обсуждать только вопросы, связанные с нашим банком.'

3. Если клиент говорит что-то нестандартное:
'Для решения вашего вопроса мне потребуется уточнить детали. Пожалуйста, оставайтесь на линии, я вас переведу на специалиста.'

Тон общения:
- Дружелюбный, но профессиональный.
- Соблюдайте спокойствие и уверенность в ответах.
- Убедитесь, что клиент чувствует, что его проблему решают оперативно.

ВАЖНО: ты ничего не придумываешь. Если в этой инструкции нет ответа на вопрос клиента, то соединяешь со специалистом:
'Для решения вашего вопроса мне потребуется уточнить детали. Пожалуйста, оставайтесь на линии, я вас переведу на специалиста.')";

int main()
{
    try {
        // Replace with your Groq API key
        const std::string api_key = "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";
        registerLLMClients();
        std::unique_ptr<LLMClient> groqClient = LLMClientFactory::instance().create("groq", { { "apiKey", api_key } });
        GroqRequest groqReq;

        groqReq.messages = { { "system", "You are a helpful assistant." },
            { "user", "Tell me a joke." } };
        groqReq.model = "gemma2-9b-it";
        auto res = groqClient->generateResponse(groqReq);
        GroqResponse *groqRes = dynamic_cast<GroqResponse *>(res.get());
        std::cout << "Groq Response: " << groqRes->choices[0].message.content
                  << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    try {
        Manager sipManager;
        PJSIPController apiController(sipManager);
        apiController.run();
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
