// Server-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <string>

const unsigned short PORT = 9999;
const std::size_t MAX_LINE_LENGTH = 15;

namespace chat {

std::string readLine(boost::asio::ip::tcp::socket &socket) {
    boost::asio::streambuf buf(MAX_LINE_LENGTH);
    boost::asio::read_until(socket, buf, "\n"); // reads until newline character is found
    std::istream is(&buf);
    std::string data;
    std::getline(is, data);
    return data;
}

void writeLine(boost::asio::ip::tcp::socket &socket, const std::string &message) {
    boost::asio::write(socket, boost::asio::buffer(message + "\n"));
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

    std::string getData() {
        return readLine(*socket_);
    }

    void sendData(const std::string &message) {
        writeLine(*socket_, message);
    }

    void run();

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::string username_;
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

    bool sendTo(const std::string &username, const std::string &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clients_.find(username);
        if (it != clients_.end()) {
            it->second->sendData(message);
            return true;
        }
        return false;
    };

    void broadcast(const std::string &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[username, session] : clients_) {
            session->sendData(message);
        }
    }

    std::string listUsers() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string user_list = "Connected users: ";
        for (const auto &[username, session] : clients_) {
            user_list += username + ", ";
        }
        if (!clients_.empty()) {
            user_list.pop_back(); // remove last space
            user_list.pop_back(); // remove last comma
        }
        return user_list;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ChatSession>> clients_;
};

void ChatSession::run() {
    manager_.broadcast(username_ + " joined the chat!");

    try {
        while (true) {
            const std::string message = getData();
            if (message == "exit") {
                manager_.broadcast(username_ + " left the chat.");
                break;
            }

            if (message.starts_with("/msg ")) {
                // private message
                size_t space_pos = message.find(' ', 5); // find the space after the username
                if (space_pos != std::string::npos) {
                    std::string target_username = message.substr(5, space_pos - 5);
                    std::string private_message = message.substr(space_pos + 1);
                    if (!manager_.sendTo(target_username, username_ + " (private): " + private_message)) {
                        sendData("Error: user '" + target_username + "' not found.");
                    }
                }
            } else if (message == "/list") {
                // list users
                sendData(manager_.listUsers());
            } else {
                // broadcast message
                manager_.broadcast(username_ + ": " + message);
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in session for " << username_ << ": " << e.what() << std::endl;
        try {
            sendData("Error: " + std::string(e.what()) + ". Connection closing.");
        } catch (...) {
            std::cerr << "Error: Failed to send error message to client." << std::endl;
        }
        manager_.broadcast(username_ + " left the chat.");
    }
}

} // namespace chat

int main(int argc, char *argv[]) {
    boost::asio::io_context io_context;

    chat::ChatServer chat_server(io_context, PORT);

    chat::ChatManager chat_manager;

    std::cout << "Server is running on port " << PORT << "..." << std::endl;

    while(true){
        // waiting for connection
        auto socket = chat_server.acceptConnection();

        try {
            // prompt for username
            chat::writeLine(*socket, "Enter your username: ");

            // get username from client
            std::string u_name = chat::readLine(*socket);

            // check if username is already taken
            while (chat_manager.sendTo(u_name, "")) {
                chat::writeLine(*socket, "Username already taken. Please choose another one. Enter your username: ");
                u_name = chat::readLine(*socket);
            }

            auto session = std::make_shared<chat::ChatSession>(socket, u_name, chat_manager);
            chat_manager.add(u_name, session);

            std::thread([session, &chat_manager, u_name]() {
                session->run();
                chat_manager.remove(u_name);
            }).detach();
        } catch (const std::exception &e) {
            std::cerr << "Error during connection setup: " << e.what() << std::endl;
            try {
                chat::writeLine(*socket, "Error: connection closed (" + std::string(e.what()) + ")");
            } catch (...) {
                std::cerr << "Error: Failed to send error message to client." << std::endl;
            }
            boost::system::error_code ec;
            socket->close(ec);
        }
    }
}