#ifndef MODULES_ROOT_CONNECTIONCORBA_HPP
#define MODULES_ROOT_CONNECTIONCORBA_HPP

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

#include <sstream>

#include "socket/ClientSocket.h"
#include "socket/SocketException.h"

/* Forward Declaration. */
namespace RTT{
    class TaskContext;
namespace Corba {
    class ControlTaskProxy;
}
}

namespace root
{
class SocketConnection : public ConnectionInterface
{
 public:
    SocketConnection(std::string sender, std::string host, int port) :
            ConnectionInterface(),
            sender(sender),
            host(host),
            port(port),
            socket(NULL),
            connected(false)
    {    
    }

    bool connect() //virtual
    {
        if(connected)
        {
            disconnect();
        }
        try{
            socket = new ClientSocket(host, port);
        } catch (SocketException e) {
            log(RTT::Info) << e.what() << RTT:endlog();
            return false;
        }
        connected = true;
        return true;
    }

    bool disconnect() //virtual
    {
        if(connected)
        {
            delete socket;
            socket = NULL;
            connected = false;
        }
    }

    std::string getReceiverName() //virtual
    {
        std::string port_str;
        std::stringstream out;
        out << port;
        return host + ":" + out.str();
    }

    std::string getSenderName() //virtual
    {
        return sender;
    }

    bool readData(std::string* data)
    {
        if(!connected)
        {
            log(RTT::Info) << "Data can not be read, no connection available." << 
                RTT:endlog();
            return false;
        }
        try{
            socket >> (*data); 
        } catch (SocketException e) {
            log(RTT::Info) << e.what() << RTT:endlog();
            return false;
        }
        return true;
    }

    bool sendData(std::string data) //virtual
    {
        if(!connected)
        {
            log(RTT::Info) << "Data can not be sent, no connection available." << 
                RTT:endlog();
            return false;
        }
        try{
            socket << data;            
        } catch (SocketException e) {
            log(RTT::Info) << e.what() << RTT:endlog();
            return false;
        }
    }

 private:
    std::string sender;
    std::string host;
    int port;
    ClientSocket* socket;
    bool connected;
};
} // namespace root
#endif
