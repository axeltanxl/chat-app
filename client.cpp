// Client-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>
#include <string>

namespace chat {

// Owns the socket and handles the raw send/receive protocol.
class ChatClient {
public:
    explicit ChatClient(boost::asio::io_context& io_context)
        : socket_(io_context) {}

    void connect(const std::string& host, unsigned short port) {
        socket_.connect(boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address(host), port));
    }

    void send(const std::string& message) {
        boost::asio::write(socket_, boost::asio::buffer(message + "\n"));
    }

    std::string receive() {
        boost::asio::streambuf buf;
        boost::asio::read_until(socket_, buf, "\n");
        std::istream is(&buf);
        std::string data;
        std::getline(is, data);
        return data;
    }

    void close() {
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
    }

private:
    boost::asio::ip::tcp::socket socket_;
};

// Owns the chat loop / console interaction, independent of the transport.
class ChatSession {
public:
    ChatSession(ChatClient& client, std::string username)
        : client_(client), username_(std::move(username)) {}

    void run() {
        client_.send(username_);

        while (true) {
            const std::string response = client_.receive();
            if (response == "exit") {
                std::cout << "Connection terminated" << std::endl;
                break;
            }
            std::cout << "Server: " << response << std::endl;

            std::cout << username_ << ": ";
            std::string reply;
            if (!std::getline(std::cin, reply) || reply == "exit") {
                client_.send("exit");
                break;
            }
            client_.send(reply);
        }
    }

private:
    ChatClient& client_;
    std::string username_;
};

} // namespace chat

int main(int argc, char* argv[]) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const unsigned short port =
        static_cast<unsigned short>(argc > 2 ? std::stoi(argv[2]) : 9999);

    try {
        boost::asio::io_context io_context;
        chat::ChatClient client(io_context);
        client.connect(host, port);

        std::cout << "Enter your name: ";
        std::string username;
        std::getline(std::cin, username);

        chat::ChatSession session(client, username);
        session.run();
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect/communicate: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
