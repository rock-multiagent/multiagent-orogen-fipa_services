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

#ifndef MTS_CONNECTIONCORBA_HPP
#define MTS_CONNECTIONCORBA_HPP

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

#include <string>

#include <rtt/InputPort.hpp>

#include "connection_interface.h"
#include <fipa_acl/fipa_acl.h>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>

/* Forward Declaration. */
namespace RTT{
    class TaskContext;
namespace corba {
    class TaskContextProxy;
}
}

namespace mts
{

extern std::string FIPA_CORBA_TRANSPORT_SERVICE;

class CorbaConnection : public ConnectionInterface
{
 public:
    CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
            std::string receiver_ior, uint32_t buffer_size = 100);
                
    ~CorbaConnection();

    /**
     * Creates and connects the ports of the sender and receiver module. 
     */
    bool connect(); //virtual

    bool disconnect(); //virtual

    fipa::acl::Letter read(); //virtual

    /**
     * Sends the data to the receiver, if the connection has been established.
     * \warning The return value will be true even if the data does not reach 
     * the receiver.
     */
    bool send(fipa::acl::Letter const& data, fipa::acl::representation::Type representation); //virtual

    /**
     * Inform the remote end about an update list of clients
     */
    void updateAgentList(const std::vector<std::string>& agents); // virtual

 private:
    CorbaConnection();
    DISALLOW_COPY_AND_ASSIGN(CorbaConnection);

    /** 
     * Creates the proxy (connection) to the other module.
     * Recreates the proxy if it already exists. 
     */
    bool createProxy();

    template<class Signature>
    std::vector<std::string> getClients();

    /**
     * Connects the sender output port to the receiver sender port.
     * This requires valid ports (on the sender and the receiver) and a valid proxy.
     */
    bool connectPorts();

 private:
    std::string mSenderIOR;
    std::string mReceiverIOR;
    std::string mInputPortName;
    std::string mOutputPortName;
    uint32_t mBufferSize;

    RTT::TaskContext* mTaskContextSender;
    RTT::corba::TaskContextProxy* mControlTaskProxy;
//    RTT::corba::CTaskContext_var mRemoteTask;
    RTT::InputPort<fipa::SerializedLetter>* mInputPort;
    RTT::OutputPort<fipa::SerializedLetter>* mOutputPort; 
    bool mPortsCreated;
    bool mProxyCreated;
    bool mReceiverConnected; // Ports are created and output port is connected.
    bool mPortsConnected;

};
} // namespace mts
#endif
