#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "agent/agent.h"
#include "core/configuration.h"
#include "db/GlobalDatabase.h"
#include "provider/provider_manager.h"
#include "server/server.h"
#include "utils/logger.h"
// Function implementations

static void runTestMode()
{
    // Initialize components
    ProviderManager::getInstance().load_providers_from_folder("./lua");
    auto result = ProviderManager::getInstance().process_request(
        "ollama",
        "Explain the difference between RAG and fine-tuning",
        {
            { "temperature", 0.5 },
            { "model", "llama3.2:1b" },
        },
        { { { "role", "system" }, { "content", "system_prompt" } } });

    LOG_DEBUG << result.response;
}

int main(int argc, char **argv)
{

    Logger::setMinLevel(Level::Debug);
    LOG_DEBUG << "TEST";
    AppConfig &config = AppConfig::getInstance();
    config.add_options({ { "test", "t", CLIParser::Type::Boolean, "test", "false" } });
    config.initialize(argc, argv);
    const bool test_mode = config.get<bool>("test");
    try {

        if (test_mode) {
            LOG_CRITICAL << "Test mode enabled";
            runTestMode();
        } else {
        Server server;
        server.run();
        }
    } catch (const std::exception &e) {
        std::cerr << "Initialization Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

//  try {
//         // Create tables (collections)
//         Table &users = db.createTable("users");
//         Table &products = db.createTable("products");
//         users.registerBeforeInsert([](const auto &id, const auto &, auto &doc) {
//             doc.insert("created_at", Value(std::time(nullptr)));
//             return true;
//         });
//         // Insert documents into users table
//         Document user1;
//         user1.insert("name", Value("Alice"));
//         user1.insert("age", Value(3.0));
//         user1.insert("email", Value("alice@example.com"));
//         users.tryInsertDocument("user_123", user1);
//         LOG_CRITICAL << users.getDocument("user_123").toJson().dump(4);

//         Document user2;
//         user2.insert("name", Value("Bob"));
//         user2.insert("age", Value(3.5));
//         user2.insert("email", Value("bob@example.com"));
//         users.tryInsertDocument("user_456", user2);

//         // Insert document into products table
//         Document product1;
//         product1.insert("name", Value("Laptop"));
//         product1.insert("price", Value(999.99));
//         product1.insert("stock", Value(5.0));
//         products.insertDocument("prod_789", product1);
//         users.deleteDocuments([](const Document &doc) {
//             auto status = doc.get("status");
//             return status && std::get<std::string>(status->get().data) == "inactive";
//         });

//         // Update all premium users
//         users.updateDocuments(
//             [](const Document &doc) {
//                 auto type = doc.get("account_type");
//                 return type && std::get<std::string>(type->get().data) == "premium";
//             },
//             [](Document &doc) {
//                 doc.insert("credits", Value(1000));
//             });
//         // Query examples
//         // Find all users over 30
//         auto seniorUsers = users.findDocumentsByValue("age", 3.5);
//         std::cout << "Found " << seniorUsers.size() << " senior users\n";
//         // Get a specific document
//         try {
//             const Document &bob = users.getDocument("user_456");
//             auto email = bob.get("email");
//             if (email) {
//                 std::cout << "Bob's email: "
//                           << std::get<std::string>(email->get().data) << "\n";
//             }
//         } catch (const std::runtime_error &e) {
//             std::cerr << "Error: " << e.what() << "\n";
//         }

//         // Update a document
//         Document updatedUser2 = users.getDocument("user_456");
//         updatedUser2.insert("age", Value(36.0));
//         users.tryUpdateDocument("user_456", updatedUser2);

//         // Delete a document
//         products.tryDeleteDocument("prod_789");

//         // Persistence examples
//         // Save entire database to directory (one subdir per table)
//         db.saveToDirectory("my_database");

//         // Save to single file
//         db.saveToFile("database_backup.bson");

//         // Load from directory
//         InMemoryDatabase restoredDb;
//         restoredDb.loadFromDirectory("my_database");

//         // Load from single file
//         InMemoryDatabase fileDb;
//         fileDb.loadFromFile("database_backup.bson");
//         auto &restoredUsers = fileDb.getTable("users");
//         std::cout << "Restored users: " << restoredUsers.size() << "\n";

//         // Table iteration example
//         std::cout << "\nAll users:\n";
//         for (const auto &[id, doc]: users) {
//             auto name = doc.get("name");
//             if (name) {
//                 std::cout << "User " << id << ": "
//                           << std::get<std::string>(name->get().data) << "\n";
//             }
//         }

//     } catch (const std::exception &e) {
//         std::cerr << "Database error: " << e.what() << "\n";
//         return 1;
//     }