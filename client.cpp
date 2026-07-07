// Client-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

class Client {
public:
    ip::tcp::socket client_socket;

    Client(io_context& io_context) : client_socket(io_context) {}

    void connect(const string& host, unsigned short port) {
        client_socket.connect(tcp::endpoint(make_address(host), port));
    }

    void sendData(const string& message) {
        write(client_socket, buffer(message + "\n"));
    }

    string getData() {
        boost::asio::streambuf buf;
        read_until(client_socket, buf, "\n");
        istream is(&buf);
        string data;
        getline(is, data);
        return data;
    }
};

int main(int argc, char* argv[])
{
    io_context io_context;
    Client client(io_context);
    client.connect("127.0.0.1", 9999);

    // Getting username from user
    cout << "Enter your name: ";
    string u_name, reply, response;
    getline(cin, u_name);

    // Sending username to another end
    // to initiate the conversation
    client.sendData(u_name);

    // Infinite loop for chit-chat
    while (true) {

        // Fetching response
        response = client.getData();

        // Validating if the connection has to be closed
        if (response == "exit") {
            cout << "Connection terminated" << endl;
            break;
        }
        cout << "Server: " << response << endl;

        // Reading new message from input stream
        cout << u_name << ": ";
        getline(cin, reply);
        client.sendData(reply);

        if (reply == "exit")
            break;
    }
    return 0;
}