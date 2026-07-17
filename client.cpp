// Client-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>

namespace chat {

// owns the socket and handles the raw send/receive protocol.
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

// owns the chat loop / console interaction, independent of the transport.
class ChatSession {
public:
    ChatSession(ChatClient& client)
        : client_(client) {}

    void run() {
        // reads broadcasts off the socket as they arrive
        std::thread reader([this]() { readLoop(); });

        std::string reply;
        while (std::getline(std::cin, reply)) {
            if (reply == "exit") {
                break;
            }
            client_.send(reply);
        }
        client_.send("exit");

        // closing the socket unblocks the reader thread's pending read.
        client_.close();
        reader.join();
    }

private:
    void readLoop() {
        try {
            while (true) {
                const std::string response = client_.receive();
                if (response == "exit") {
                    std::cout << "\nConnection terminated" << std::endl;
                    break;
                }
                std::cout << "\n" << response << std::endl;
            }
        } catch (const std::exception&) {
            
        }
    }

    ChatClient& client_;
};

} // namespace chat

int main(int argc, char* argv[]) {
    const std::string host = "127.0.0.1";
    const unsigned short port =
        static_cast<unsigned short>(argc > 2 ? std::stoi(argv[2]) : 9999);

    try {
        boost::asio::io_context io_context;
        chat::ChatClient client(io_context);
        client.connect(host, port);

        chat::ChatSession session(client);
        session.run();
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect/communicate: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
