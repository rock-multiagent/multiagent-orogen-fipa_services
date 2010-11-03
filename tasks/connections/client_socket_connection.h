/*
 * \file    
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

#ifndef MODULES_ROOT_CONNECTIONCORBA_HPP
#define MODULES_ROOT_CONNECTIONCORBA_HPP

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

#include <sstream>

#include "connection_interface.h"
#include "astrium-proxy/socket/ClientSocket.h"
#include "astrium-proxy/socket/SocketException.h"

namespace root
{
namespace dcp = dfki::communication::proxy;

class ClientSocketConnection : public ConnectionInterface
{
 public:
    ClientSocketConnection(std::string sender, std::string host, int port);

    bool connect(); //virtual

    bool disconnect(); //virtual

    std::string read();

    bool send(std::string const& data); //virtual

 private:
    std::string host;
    int port;
    dcp::ClientSocket* socket;
    bool connected;
};
} // namespace root
#endif
