/*
 * \file    connection_interface.h
 *  
 * \brief   
 *
 * \details 
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    27.09.2010
 *        
 * \version 0.1 
 *          
 *
 * \author  Stefan.Haase@dfki.de
 */

#ifndef MODULES_ROOT_CONNECTIONINTERFACE_HPP
#define MODULES_ROOT_CONNECTIONINTERFACE_HPP

#include <stdexcept>
#include <string>
#include <vector>

namespace root
{
class ConnectionException : public std::runtime_error
{
 public:
    ConnectionException(const std::string& msg) : runtime_error(msg)
    {
    }
};

/**
 * Connection interface between different modules
 * and betweeen modules and socket servers.
 *
 * \todo Find a way to use globalLogging.
 */
class ConnectionInterface
{
 public:
    ~ConnectionInterface(){};
    virtual bool connect()=0;
    virtual bool disconnect()=0;
    inline std::string getReceiverName(){return receiverName;};
    inline std::string getSenderName(){return senderName;};
    inline bool initialize(){return true;};
    inline bool isConnected(){return connected;};
    virtual std::string read()=0;
    virtual bool send(std::string const& data)=0;

 protected:
    ConnectionInterface(){};

    std::string senderName;
    std::string receiverName;
    bool connected;
};
} // namespace root

#endif
