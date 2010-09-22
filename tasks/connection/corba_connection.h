#ifndef MODULES_ROOT_CONNECTIONCORBA_HPP
#define MODULES_ROOT_CONNECTIONCORBA_HPP

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

#include <rtt/Ports.hpp>

/* Forward Declaration. */
namespace RTT{
    class TaskContext;
namespace Corba {
    class ControlTaskProxy;
}
}

namespace root
{
typedef <bool(std::string const &, std::string const &)>

class ConnectionCorba : public ConnectionInterface
{
 public:
    ConnectionCorba(RTT::TaskContext* sender, std::string receiver, 
            str::string receiver_ior) : 
            ConnectionInterface(),
            taskContextSender(sender),
            receiverName(receiver),
            receiverIOR(receiver_ior),
            portsCreated(false),
            proxyCreated(false),
    {
        senderName = task_context_local->getName();
        senderIOR = RTT::Corba::ControlTaskServer::getIOR(taskContext);
        inputPortName = receiverName + "->" + senderName + "_port";    
        outputPortName = senderName + "->" + receiverName + "_port";
    }
                
    ~ConnectionCorba()
    {
        disconnect();
    }
    bool connect(){}; //virtual
    bool disconnect(){}; //virtual
    std::string getSenderName(){}; //virtual
    std::string getReceiverName(){}; //virtual
    bool initialize(){}; //virtual
    bool sendData(std::vector<uint8_t> data){}; //virtual

 private:
    ConnectionCorba();
    DISALLOW_COPY_AND_ASSIGN(ConnectionInterface);

    bool createPorts()
    {
        // Do nothing if the ports have already been created.
        if(portsCreated)
            return false;

        std::string info_input = "Input port from '"+receiverName+"' to '"+senderName+"'";
        std::string info_output = "Input port from '"+senderName+"' to '"+receiverName+"'";
        std::string create_input = "Create InputPort '" + inputPortName + "'";
        std::string create_output = "Create OutputPort '" + outputPortName + "'";
        std::string event_input = "Port '" + inputPortName + "' connected to an event handler.";

        inputPort = new RTT::InputPort<fipa::BitefficientMessage>(inputPortName);
        outputPort = new RTT::OutputPort<fipa::BitefficientMessage>(outputPortName);

        if(taskContext->ports()->addEventPort(inputPort, info_input)) {
            log(RTT::Info) << create_input << RTT::endlog();
            if (taskContext->connectPortToEvent(inputPort))
                log(RTT::Info) << event_input << RTT::endlog();
            else
                log(RTT::Error) << event_input << RTT::endlog();
        } else {
            log(RTT::Error) << create_input << RTT::endlog();
            return false;
        }

        if(taskContext->ports()->addPort(outputPort, info_output)) {
            log(RTT::Info) << create_output << RTT::endlog();
        } else {
            log(RTT::Error) << create_output << RTT::endlog();
            return false;
        }
        portsCreated = true;
        return true;
    }

    /** 
     * Creates the proxy (connection) to the other module.
     * Recreates the proxy if it already exists. 
     */
    bool createProxy()
    {
        proxyCreated = false;
        if(controlTaskProxy != NULL)
        {
            taskContextSender->removePeer(receiverIOR);
            delete controlTaskProxy;
            controlTaskProxy = NULL;
            log(RTT::Info) << "Delete ControlTaskProxy to '" <<  
                    rceiverName << "'" << RTT::endlog();
        }
        // Create Control Task Proxy.
        RTT::Corba::ControlTaskProxy::InitOrb(0, 0);
        controlTaskProxy = RTT::Corba::ControlTaskProxy::
                Create(receiverIOR, receiverIOR.substr(0,3) == "IOR");
        // Creating a one-directional connection from task_context to the peer. 
        if(taskContextSender->addPeer(controlTaskProxy))
            log(RTT::Info) << "Create ControlTaskProxy to '" <<  
                    rceiverName << "'" << RTT::endlog();
        else 
        {
             log(RTT::Error) << "Can not create ControlTaskProxy to '" <<  
                    receiverName << "'" << RTT::endlog();
            return false;
        }
        proxyCreated = true;
        return true;
    }

    /**
     * Creates and connects the ports on the receiver-module.
     * This requires valid ports and a valid proxy.
     * \param function_name Name of the orogen function on the 
     * receiver-module which creates the ports and the connection. 
     * \param Signature E.g. bool(std::string const&, std::string const &).
     */
    template<class Signature>
    bool createConnectPortsOnReceiver(std::string function_name)
    {
        receiverConnected = false;
        if(!portsCreated || !proxyCreated)
            return false;
        
        // Get function matching the Signature and the name.
        RTT::Method <Signature> create_receiver_ports = 
            controlTaskProxy->methods()->getMethod<Signature>(function_name);

        // Receiver function is ready?
        if(!create_receiver_ports.ready())
        {
            globalLog(RTT::Error, "Connection on the remote module '%s' could not be created", 
                    se.getServiceDescription().getName().c_str());
            return false;
        }
        // Create receiver connection.
        if(!create_receiver_ports(senderName, senderIOR))
        {
            globalLog(RTT::Error, "Connection on the remote module '%s' could not be created", 
                    se.getServiceDescription().getName().c_str());
            return false;
        }
        // Refresh control task proxy to get to know the new receiver ports.
        if(createProxy()) {
            receiverConnected = true;
            return true;
        } else {
            return false;
        }
    }

    /**
     * Connects the sender output port to the receiver sender port.
     * This requires valid ports (on the sender and the receiver) and a valid proxy.
     */
    bool connectPorts()
    {
        if(!portsCreated || !proxyCreated || !receiverConnected)
            return false;

        // Get pointer to the input port of the receiver.
        RTT::InputPortInterface* remoteinputport = NULL;
        remoteinputport = (RTT::InputPortInterface*)controlTaskProxy->ports()->
                getPort(outputPortName);
        if(remoteinputport == NULL)
        {
            globalLog(RTT::Error, "Inputport '%s' of the receiver module '%s' could not be resolved",
                outputPortName.c_str(), receiverName.c_str());
            return false;
        }
  
        // Connect the output porrt of the sender to the input port of the receiver.
        // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
        // true=pull(problem here) false=push)
        if(!outputPort->connectTo(*remoteinputport, 
                RTT::ConnPolicy::buffer(20, RTT::ConnPolicy::LOCKED, false, false)))
        {
            globalLog(RTT::Error, "Outputport '%s' cant be connected to the input port of %s",
                outputPortName.c_str(), receiverName.c_str());
            return false;
        }
        globalLog(RTT::Info, "Connected to '%s'", receiverName.c_str());
        return true;
    }

 private:
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
    bool isInitialized;
    bool portsCreated;
    bool proxyCreated;
    bool receiverConnected; // Ports are created and output port is connected.
};
} // namespace root
#endif
