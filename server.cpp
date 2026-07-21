#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <unordered_map>
#include <unordered_set>

const unsigned short PORT = 9999;
const std::size_t MAX_LINE_LENGTH = 1024;

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

class ChatServer;

class ChatSession {
public:
    ChatSession(std::shared_ptr<boost::asio::ip::tcp::socket> socket, std::string username, ChatServer& server)
        : socket_(std::move(socket)), username_(std::move(username)), server_(server) {}

    std::string getData() {
        return readLine(*socket_);
    }

    void sendData(const std::string &message) {
        writeLine(*socket_, message);
    }

    void run();

    void setRoom(const std::string &room) {
        room_ = room;
    }

    std::string getRoom() {
        return room_;
    }

private:
    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::string username_;
    std::string room_;
    ChatServer& server_;
};

class ChatServer {
public:
    ChatServer(boost::asio::io_context &io_context, unsigned short port)
        : io_context_(io_context), acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
        rooms_["default"] = {};
    }

    std::shared_ptr<boost::asio::ip::tcp::socket> acceptConnection() {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
        acceptor_.accept(*socket);
        return socket;
    }

    void add(const std::string &username, std::shared_ptr<ChatSession> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[username] = session;
        rooms_["default"].insert(username);
        session->setRoom("default");
    };

    void remove(const std::string &username) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(username);
        for (auto &[room_name, users] : rooms_) {
            users.erase(username);
        }
    };

    bool exists(const std::string &username) {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.find(username) != clients_.end();
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

    // broadcasts a message to all clients in the same room as the sender
    void broadcast(const std::string &sender, const std::string &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto sender_it = clients_.find(sender);
        if (sender_it != clients_.end()) {
            const std::string &room_name = sender_it->second->getRoom();
            auto room_it = rooms_.find(room_name);
            if (room_it != rooms_.end()) {
                for (const auto &username : room_it->second) {
                    try {
                        clients_[username]->sendData(message);
                    } catch (const std::exception &e) {
                        std::cerr << "Error: failed to send to " << username << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }

    void createRoom(const std::string &room_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        rooms_[room_name] = {};
    }

    bool joinRoom(const std::string &username, const std::string &room_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto room_it = rooms_.find(room_name);
        if (room_it != rooms_.end()) {
            // remove user from their current room
            for (auto &[r_name, users] : rooms_) {
                users.erase(username);
            }
            // add user to the new room
            room_it->second.insert(username);
            clients_[username]->setRoom(room_name);
            return true;
        }
        return false;
    }

    std::string listRooms() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string room_list = "Available rooms: ";
        for (const auto &[room_name, users] : rooms_) {
            room_list += room_name + ", ";
        }
        if (!rooms_.empty()) {
            room_list.pop_back(); // remove last space
            room_list.pop_back(); // remove last comma
        }
        return room_list;
    }

    bool leaveRoom(const std::string &username, const std::string &room_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto room_it = rooms_.find(room_name);
        if (room_it != rooms_.end()) {
            room_it->second.erase(username);
            return true;
        }
        return false;
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

    std::string listUsersInRoom(const std::string &username) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto user_it = clients_.find(username);
        if (user_it != clients_.end()) {
            const std::string &room_name = user_it->second->getRoom();
            auto room_it = rooms_.find(room_name);
            if (room_it != rooms_.end()) {
                std::string user_list = "Users in room '" + room_name + "': ";
                for (const auto &user : room_it->second) {
                    user_list += user + ", ";
                }
                if (!room_it->second.empty()) {
                    user_list.pop_back(); // remove last space
                    user_list.pop_back(); // remove last comma
                }
                return user_list;
            }
        }
        return "Error: could not find the user's room.";
    }

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ChatSession>> clients_;
    std::unordered_map<std::string, std::unordered_set<std::string>> rooms_;
};

void ChatSession::run() {
    server_.broadcast(username_, username_ + " joined the chat!");

    try {
        while (true) {
            const std::string message = getData();
            if (message.starts_with("/msg ")) {
                // private message
                size_t space_pos = message.find(' ', 5);
                if (space_pos != std::string::npos) {
                    std::string target_username = message.substr(5, space_pos - 5);
                    std::string private_message = message.substr(space_pos + 1);
                    if (!server_.sendTo(target_username, username_ + " (private): " + private_message)) {
                        sendData("Error: user '" + target_username + "' not found.");
                    }
                }
            } else if (message.starts_with("/join ")) {
                // join room
                std::string room_name = message.substr(6);
                if (server_.joinRoom(username_, room_name)) {
                    sendData("Joined room '" + room_name + "'.");
                } else {
                    sendData("Error: room '" + room_name + "' does not exist.");
                }
            } else if (message.starts_with("/create ")) {
                // create room
                std::string room_name = message.substr(8);
                server_.createRoom(room_name);
                server_.joinRoom(username_, room_name);
                sendData("Created and joined room '" + room_name + "'.");
            } else if (message.starts_with("/leave")) {
                // leave current room
                std::string current_room = getRoom();
                if (current_room != "default" && server_.leaveRoom(username_, current_room)) {
                    server_.joinRoom(username_, "default");
                    sendData("Left room '" + current_room + "'. Now in default room.");
                } else {
                    sendData("Error: you are not in a room or already in the default room.");
                }
            } else if (message == "/users") {
                // list all users in the same room
                sendData(server_.listUsersInRoom(username_));
            } else if (message == "/rooms") {
                // list rooms
                sendData(server_.listRooms());
            } else if (message == "/help") {
                // help command
                sendData("Your username is " + username_ + ". \\n"
                         "You are in room '" + getRoom() + "'. \\n"
                         "Available commands:\\n"
                         "/msg <username> <message> - Send a private message to a user\\n"
                         "/join <room_name> - Join a room\\n"
                         "/create <room_name> - Create a new room\\n"
                         "/leave - Leave the current room\\n"
                         "/users - List users in the current room\\n"
                         "/rooms - List all available rooms\\n"
                         "/help - Show this help message\\n"
                         "/exit - Disconnect from the chat");
            } else if (message == "/exit") {
                server_.leaveRoom(username_, getRoom());
                server_.broadcast(username_, username_ + " left the chat.");
                break;
            } else {
                // broadcast message
                server_.broadcast(username_, username_ + ": " + message);
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in session for " << username_ << ": " << e.what() << std::endl;
        try {
            sendData("Error: " + std::string(e.what()) + ". Connection closing.");
        } catch (...) {
            std::cerr << "Error: Failed to send error message to client." << std::endl;
        }
    }
}

} // namespace chat

int main(int argc, char *argv[]) {
    boost::asio::io_context io_context;

    chat::ChatServer chat_server(io_context, PORT);

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
            while (chat_server.exists(u_name)) {
                chat::writeLine(*socket, "Username already taken. Please choose another one.\\nEnter your username: ");
                u_name = chat::readLine(*socket);
            }

            auto session = std::make_shared<chat::ChatSession>(socket, u_name, chat_server);
            chat_server.add(u_name, session);

            std::thread([session, &chat_server, u_name]() {
                session->run();
                chat_server.remove(u_name);
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