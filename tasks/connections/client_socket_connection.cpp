#include "client_socket_connection.h"

namespace root
{
ClientSocketConnection::ClientSocketConnection(std::string sender, std::string host, int port) :
        ConnectionInterface(),
        senderName(sender),
        host(host),
        port(port),
        socket(NULL),
        connected(false)
{    
}

bool ClientSocketConnection::connect() //virtual
{
    if(connected)
    {
        disconnect();
    }
    try{
        socket = new dcp::ClientSocket(host, port);
    } catch (dcp::SocketException& e) {
        throw ConnectionException("SocketException: "+e.description());
        return false;
    }
    connected = true;
    return true;
}

bool ClientSocketConnection::disconnect() //virtual
{
    if(connected)
    {
        delete socket;
        socket = NULL;
        connected = false;
    }
}

std::string ClientSocketConnection::getReceiverName() //virtual
{
    std::string port_str;
    std::stringstream out;
    out << port;
    return host + ":" + out.str();
}

std::string ClientSocketConnection::read()
{
    if(!connected)
    {
        throw ConnectionException("Data can not be read, no connection available.");
        return false;
    }
    std::string data;
    try{
        *socket >> data; 
    } catch (dcp::SocketException& e) {
        throw ConnectionException("SocketException: "+e.description());
    }
    return data;
}

bool ClientSocketConnection::send(std::string const& data) //virtual
{
    if(!connected)
    {
        throw ConnectionException("Data can not be sent, no connection available.");
        return false;
    }
    try{
        *socket << data;            
    } catch (dcp::SocketException& e) {
        throw ConnectionException("SocketException: "+e.description());
        return false;
    }
    return true;
}
} // namespace root
