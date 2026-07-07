// Client-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>
using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

string getData(tcp::socket& socket)
{
    boost::asio::streambuf buf;
    read_until(socket, buf, "\n");
    istream is(&buf);
    string data;
    getline(is, data);
    return data;
}

void sendData(tcp::socket& socket, const string& message)
{
    write(socket,
          buffer(message + "\n"));
}

int main(int argc, char* argv[])
{
    io_context io_context;
    // socket creation
    ip::tcp::socket client_socket(io_context);

    client_socket
        .connect(
            tcp::endpoint(
                make_address("127.0.0.1"),
                9999));

    // Getting username from user
    cout << "Enter your name: ";
    string u_name, reply, response;
    getline(cin, u_name);

    // Sending username to another end
    // to initiate the conversation
    sendData(client_socket, u_name);

    // Infinite loop for chit-chat
    while (true) {

        // Fetching response
        response = getData(client_socket);

        // Validating if the connection has to be closed
        if (response == "exit") {
            cout << "Connection terminated" << endl;
            break;
        }
        cout << "Server: " << response << endl;

        // Reading new message from input stream
        cout << u_name << ": ";
        getline(cin, reply);
        sendData(client_socket, reply);

        if (reply == "exit")
            break;
    }
    return 0;
}