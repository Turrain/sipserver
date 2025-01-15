// abstract_websocket_client.h
#pragma once

#include <functional>
#include <string>
#include <thread>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

class AbstractWebSocketClient {
protected:
    using Client = websocketpp::client<websocketpp::config::asio_client>;
    using MessagePtr = websocketpp::config::asio_client::message_type::ptr;

    Client client;
    websocketpp::connection_hdl connection;
    bool connected { false };
    std::thread client_thread;

    AbstractWebSocketClient()
    {
        client.init_asio();
        client.clear_access_channels(websocketpp::log::alevel::none);
        client.clear_error_channels(websocketpp::log::elevel::none);
        client.set_error_channels(websocketpp::log::elevel::none);
        client.set_access_channels(websocketpp::log::alevel::none);
        
        client.set_message_handler([this](auto hdl, auto msg) {
            on_message(hdl, msg);
        });

        client.set_open_handler([this](auto hdl) {
            connected = true;
            on_open(hdl);
        });

        client.set_close_handler([this](auto hdl) {
            connected = false;
            on_close(hdl);
        });
    }

public:
    virtual ~AbstractWebSocketClient()
    {
        disconnect();
        if (client_thread.joinable()) {
            client.stop();
            client_thread.join();
        }
    }

    void connect(const std::string &uri)
    {
        websocketpp::lib::error_code ec;
        auto conn = client.get_connection(uri, ec);
        if (ec) {
            on_error(ec.message());
            return;
        }

        connection = conn->get_handle();
        client.connect(conn);

        client_thread = std::thread([this]() {
            try {
                client.run();
            } catch (const std::exception &e) {
                on_error(e.what());
            }
        });
    }

    void disconnect()
    {
        if (connected) {
            client.close(connection, websocketpp::close::status::normal, "");
        }
    }

    bool is_connected() const { return connected; }

protected:
    // Pure virtual methods to be implemented by derived classes
    virtual void on_message(websocketpp::connection_hdl hdl, MessagePtr msg) = 0;
    virtual void on_open(websocketpp::connection_hdl hdl) = 0;
    virtual void on_close(websocketpp::connection_hdl hdl) = 0;
    virtual void on_error(const std::string &error) = 0;
};

