// Server-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>

namespace chat {

class ChatServer {

  public:
    ChatServer(boost::asio::io_context& io_context, unsigned short port)
        : acceptor_server_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
          server_socket_(io_context) {}

    // Driver program for receiving data from buffer
    std::string getData()
    {
        boost::asio::streambuf buf;
        boost::asio::read_until(server_socket_, buf, "\n");
        std::istream is(&buf);
        std::string data;
        std::getline(is, data);
        return data;
    }

    // Driver program to send data
    void sendData(const std::string& message)
    {
        boost::asio::write(server_socket_, boost::asio::buffer(message + "\n"));
    }

    void acceptConnection() {
        acceptor_server_.accept(server_socket_);
    }


  private:
    boost::asio::ip::tcp::acceptor acceptor_server_;
    boost::asio::ip::tcp::socket server_socket_;
};

class ChatSession {
  public:
    ChatSession(ChatServer& server, std::string username)
        : server_(server), username_(std::move(username)) {}

    void run() {
        // Replying with default message to initiate chat
        std::string response, reply;
        reply = "Hello " + username_ + "!";
        std::cout << "Server: " << reply << std::endl;
        server_.sendData(reply);

        while (true) {
            const std::string response = server_.getData();
            if (response == "exit") {
                std::cout << username_ << " left!" << std::endl;
                break;
            }
            std::cout << username_ << ": " << response << std::endl;

            // Reading new message from input stream
            std::cout << "Server"
                      << ": ";
            std::getline(std::cin, reply_);
            server_.sendData(reply_);

            if (reply_ == "exit")
                break;
        }
    }

  private:
    ChatServer& server_;
    std::string username_;
    std::string reply_;
};

} // namespace chat

int main(int argc, char* argv[])
{
    boost::asio::io_context io_context;

    chat::ChatServer chat_server(io_context, 9999);

    // waiting for connection
    chat_server.acceptConnection();

    // Reading username
    std::string u_name = chat_server.getData();

    chat::ChatSession session(chat_server, u_name);
    session.run();
}