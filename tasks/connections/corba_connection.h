/*
 * \file    corba_connection.h
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

#include <string>

#include <rtt/InputPort.hpp>

#include "connections/connection_interface.h"
#include "connections/fipa_msg_object.h"

/* Forward Declaration. */
namespace RTT{
    class TaskContext;
namespace corba {
    class TaskContextProxy;
}
}

namespace root
{
class CorbaConnection : public ConnectionInterface
{
 public:
    CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
            std::string receiver_ior);
                
    ~CorbaConnection();

    /**
     * Creates and connects the ports of the sender and receiver module. 
     */
    bool connect(); //virtual

    /**
     * Create and connect the ports of this local module.
     * Same as connect() without createConnectPortsOnReceiver().
     */
    bool connectLocal();

    bool disconnect(); //virtual

    std::string read(); //virtual

    /**
     * Sends the data to the receiver, if the connection has been established.
     * \warning The return value will be true even if the data does not reach 
     * the receiver.
     */
    bool send(std::string const& data); //virtual

 private:
    CorbaConnection();
    DISALLOW_COPY_AND_ASSIGN(CorbaConnection);

    bool createPorts();

    /** 
     * Creates the proxy (connection) to the other module.
     * Recreates the proxy if it already exists. 
     */
    bool createProxy();

    /**
     * Creates and connects the ports on the receiver-module.
     * This requires valid ports and a valid proxy.
     * \param function_name Name of the orogen function on the 
     * receiver-module which creates the ports and the connection. 
     * \param Signature E.g. bool(std::string const&, std::string const &).
     */
    template<class Signature>
    bool createConnectPortsOnReceiver(std::string function_name);

    /**
     * Connects the sender output port to the receiver sender port.
     * This requires valid ports (on the sender and the receiver) and a valid proxy.
     */
    bool connectPorts();

 private:
    std::string senderIOR;
    std::string receiverIOR;
    std::string inputPortName;
    std::string outputPortName;

    RTT::TaskContext* taskContextSender;
    RTT::corba::TaskContextProxy* controlTaskProxy;
    RTT::InputPort<fipa::BitefficientMessage>* inputPort;
    RTT::OutputPort<fipa::BitefficientMessage>* outputPort; 
    bool portsCreated;
    bool proxyCreated;
    bool receiverConnected; // Ports are created and output port is connected.
    bool portsConnected;
};
} // namespace root
#endif
