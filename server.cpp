// Server-side Synchronous Chatting Application
// using C++ boost::asio

#include <boost/asio.hpp>
#include <iostream>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

// Driver program for receiving data from buffer
string getData(tcp::socket& socket)
{
    boost::asio::streambuf buf;
    read_until(socket, buf, "\n");
    istream is(&buf);
    string data;
    getline(is, data);
    return data;
}

// Driver program to send data
void sendData(tcp::socket& socket, const string& message)
{
    write(socket,
          buffer(message + "\n"));
}

int main(int argc, char* argv[])
{
    io_context io_context;

    // Listening for any new incomming connection
    // at port 9999 with IPv4 protocol
    tcp::acceptor acceptor_server(
        io_context,
        tcp::endpoint(tcp::v4(), 9999));

    // Creating socket object
    tcp::socket server_socket(io_context);

    // waiting for connection
    acceptor_server.accept(server_socket);

    // Reading username
    string u_name = getData(server_socket);

    // Replying with default message to initiate chat
    string response, reply;
    reply = "Hello " + u_name + "!";
    cout << "Server: " << reply << endl;
    sendData(server_socket, reply);

    while (true) {

        // Fetching response
        response = getData(server_socket);

        // Validating if the connection has to be closed
        if (response == "exit") {
            cout << u_name << " left!" << endl;
            break;
        }
        cout << u_name << ": " << response << endl;

        // Reading new message from input stream
        cout << "Server"
             << ": ";
        getline(cin, reply);
        sendData(server_socket, reply);

        if (reply == "exit")
            break;
    }
    return 0;
}