#ifndef MODULES_ROOT_CONNECTIONINTERFACE_HPP
#define MODULES_ROOT_CONNECTIONINTERFACE_HPP

#include <string>
#include <vector>

#include <rtt/Logger.hpp>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

namespace root
{
/**
 * Connection interface between different modules
 * and betweeen modules and socket servers.
 *
 * \todo Find a way to use globalLogging.
 */
class ConnectionInterface
{
 public:
    ConnectionInterface()
    {
        if (RTT::log().getLogLevel() < RTT::Logger::Info)
        {
            RTT::log().setLogLevel(RTT::Logger::Info);
        }
    };
    virtual bool connect();
    virtual bool disconnect();
    virtual std::string getReceiverName();
    virtual std::string getSenderName();
    virtual bool initialize(){};
    virtual bool readData(std::string* data);
    virtual bool sendData(std::string data);

 private:
    DISALLOW_COPY_AND_ASSIGN(ConnectionInterface);
};
} // namespace root

#endif
