#pragma once
#include <asio.hpp>
#include <memory>
#include "chatapp/connection.hpp"
 
namespace chatapp {
 
class ChatClient {
public:
    ChatClient(asio::io_context& io, const std::string& host, unsigned short port);
 
    void sendMessage(const std::string& text);
 
private:
    asio::io_context& io_;
    std::shared_ptr<Connection> conn_;
};
 
} // namespace chatapp
 