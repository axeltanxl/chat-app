#include <asio.hpp>
#include <iostream>
#include <thread>
#include <string>
#include "chatapp/chat_client.hpp"
 
int main(int argc, char* argv[]) {
    try {
        std::string host = argc > 1 ? argv[1] : "127.0.0.1";
        unsigned short port = argc > 2 ? static_cast<unsigned short>(std::stoi(argv[2])) : 8080;
 
        asio::io_context io;
        chatapp::ChatClient client(io, host, port);
 
        // run io_context on a background thread so we can read stdin on the main thread
        std::thread ioThread([&io]() { io.run(); });
 
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "/quit") break;
            client.sendMessage(line);
        }
 
        io.stop();
        ioThread.join();
    } catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}