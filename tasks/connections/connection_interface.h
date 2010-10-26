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
    virtual bool connect();
    virtual bool disconnect();
    virtual std::string getReceiverName();
    virtual std::string getSenderName();
    virtual bool initialize(){};
    virtual std::string readData();
    virtual bool sendData(std::string const& data);

 protected:
    ConnectionInterface(){};
};
} // namespace root

#endif
