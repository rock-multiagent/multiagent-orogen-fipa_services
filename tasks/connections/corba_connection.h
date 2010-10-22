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

/* Forward Declaration. */
namespace RTT{
    class TaskContext;
namespace Corba {
    class ControlTaskProxy;
}
}

namespace root
{
// TODO change name to CorbaFipaConnection or corbaHighLevelConnection...
class CorbaConnection : public ConnectionInterface
{
 public:
    CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
            str::string receiver_ior) : 
            ConnectionInterface(),
            taskContextSender(sender),
            receiverName(receiver),
            receiverIOR(receiver_ior),
            controlTaskProxy(NULL),
            inputPort(NULL),
            outputPort(NULL),
            portsCreated(false),
            proxyCreated(false),
            receiverConnected(false),
            connected(false)
    {
    }
                
    ~CorbaConnection()
    {
    }

    /**
     * Creates and connects the ports of the sender and receiver module. 
     */
    bool connect(); //virtual

    bool disconnect() //virtual
    {
    }

    std::string getSenderName() //virtual
    {
        return senderName;
    }

    std::string getReceiverName() //virtual
    {
        return receiverName;
    }

    bool readData(std::string* data); //virtual

    /**
     * Sends the data to the receiver, if the connection has been established.
     * \warning The return value will be true even if the data does not reach 
     * the receiver.
     */
    bool sendData(std::string data); //virtual

 private:
    ConnectionCorba();
    DISALLOW_COPY_AND_ASSIGN(ConnectionInterface);

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
    CorbaConnection(){}

    std::string senderName;
    std::string senderIOR;
    std::string receiverName;
    std::string receiverIOR;
    std::string inputPortName;
    std::string outputPortName;

    RTT::TaskContext* taskContextSender;
    RTT::Corba::ControlTaskProxy* controlTaskProxy;
    RTT::InputPort<fipa::BitefficientMessage>* inputPort;
    RTT::OutputPort<fipa::BitefficientMessage>* outputPort; 
    bool portsCreated;
    bool proxyCreated;
    bool receiverConnected; // Ports are created and output port is connected.
    bool connected;
};
} // namespace root
#endif
