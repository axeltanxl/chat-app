// Server-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>
#include <thread>

namespace chat {

// Driver program for receiving data from buffer
std::string readLine(boost::asio::ip::tcp::socket &socket) {
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, "\n"); // reads until newline character is found
    std::istream is(&buf);
    std::string data;
    std::getline(is, data);
    return data;
}

class ChatServer {
public:
    ChatServer(boost::asio::io_context &io_context, unsigned short port)
        : io_context_(io_context), acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {}

    std::shared_ptr<boost::asio::ip::tcp::socket> acceptConnection() {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        acceptor_.accept(*socket);
        return socket;
    }

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

class ChatManager;

class ChatSession {
public:
    ChatSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, std::string username, ChatManager& manager)
        : socket_(std::move(socket)), username_(std::move(username)), manager_(manager) {}

    // Driver program for receiving data from buffer
    std::string getData() {
        return readLine(*socket_);
    }

    // Driver program to send data
    void sendData(const std::string &message) {
        boost::asio::write(*socket_, boost::asio::buffer(message + "\n"));
    }

    void run();   // defined below, once ChatManager is fully declared

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::string username_;
    std::string reply_;
    ChatManager& manager_;
};

class ChatManager {
public:
    void add(const std::string &username, std::shared_ptr<ChatSession> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[username] = session;
    };

    void remove(const std::string &username) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(username);
    };

    void sendTo(const std::string &username, const std::string &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clients_.find(username);
        if (it != clients_.end()) {
            it->second->sendData(message);
        }
    };

    void broadcast(const std::string &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[username, session] : clients_) {
            session->sendData(message);
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ChatSession>> clients_;
};

// ChatManager is complete now, so ChatSession::run() can call manager_.broadcast(...)
void ChatSession::run() {
    manager_.broadcast(username_ + " joined the chat!");

    while (true) {
        const std::string message = getData();
        if (message == "exit") {
            manager_.broadcast(username_ + " left the chat!");
            break;
        }
        // std::cout << username_ << ": " << message << std::endl;
        manager_.broadcast(username_ + ": " + message);
    }
}

} // namespace chat

int main(int argc, char *argv[]) {
    boost::asio::io_context io_context;

    chat::ChatServer chat_server(io_context, 9999);

    chat::ChatManager chat_manager;

    std::cout << "Server is running on port 9999..." << std::endl;

    while(true){
        // waiting for connection
        auto socket = chat_server.acceptConnection();

        // get username from client
        std::string u_name = chat::readLine(*socket);

        auto session = std::make_shared<chat::ChatSession>(socket, u_name, chat_manager);

        chat_manager.add(u_name, session);

        std::thread([session, &chat_manager, u_name]() {
            session->run();
            chat_manager.remove(u_name);
        }).detach();
    }
}