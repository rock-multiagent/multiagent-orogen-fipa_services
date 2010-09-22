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
class CorbaConnection : public ConnectionInterface
{
 public:
    CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
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
                
    ~CorbaConnection()
    {
        disconnect();
    }

    /**
     * Creates and connects the ports of the sender and receiver module. 
     */
    bool connect() //virtual
    {
        if(!createPorts())
        {
            log(RTT::Info) << "Sender ports could not be created." << RTT::endlog();
            return false;
        }

        if(!createProxy())
        {
            log(RTT::Info) << "Sender proxy could not be created." << RTT::endlog();
            return false;
        }

        if(!createConnectPortsOnReceiver<bool(std::string const&, std::string const &)>
                ("rpcCreateConnectPorts"))
        {
            log(RTT::Info) << "Receiver ports could not be created/connected." << 
                    RTT::endlog();
            return false;
        }

        if(!connectPorts())
        {
            log(RTT::Info) << "Sender ports could not be connected." << RTT::endlog();
            return false;
        }
        log(RTT::Info) << "Modules " << senderName << " and " << receiverName << 
                "connected." << RTT::endlog()
    }

    bool disconnect(){}; //virtual

    std::string getSenderName() //virtual
    {
        return senderName;
    }

    std::string getReceiverName() //virtual
    {
        return receiverName;
    }

    bool readData(std::string* data) //virtual
    {
        fipa::BitefficientMessage msg;
        bool ret = inputPort->read(msg);
        if(ret)
        {
            *data = msg.toString();
        }
        return ret;
    }

    /**
     * Sends the data to the receiver, if the connection has been established.
     * \warning The return value will be true even if the data does not reach 
     * the receiver.
     */
    bool sendData(std::string data) //virtual
    {
        if(!connected)
        {
            log(RTT::Info) << "Data can not be sent, no connection available. " << 
                RTT::endlog();
            return false;
        }
        // string to char-vector conversion to handle zeros within the string. 
        fipa::BitefficientMessage msg(data);
        outputPort->write(msg);
        return true;
    }

 private:
    ConnectionCorba();
    DISALLOW_COPY_AND_ASSIGN(ConnectionInterface);

    bool createPorts()
    {
        // Do nothing if the ports have already been created.
        if(portsCreated)
            return false;

        inputPort = new RTT::InputPort<fipa::BitefficientMessage>(inputPortName);
        outputPort = new RTT::OutputPort<fipa::BitefficientMessage>(outputPortName);

        if(!taskContext->ports()->addEventPort(inputPort, info_input) ||
            !taskContext->connectPortToEvent(inputPort)) 
            return false;

        if(!taskContext->ports()->addPort(outputPort, info_output))
            return false;
     
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
        }

        // Create Control Task Proxy.
        RTT::Corba::ControlTaskProxy::InitOrb(0, 0);
        controlTaskProxy = RTT::Corba::ControlTaskProxy::
                Create(receiverIOR, receiverIOR.substr(0,3) == "IOR");

        // Creating a one-directional connection from task_context to the peer. 
        if(!taskContextSender->addPeer(controlTaskProxy))
            return false;
        
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
            return false;
     
        // Create receiver connection.
        if(!create_receiver_ports(senderName, senderIOR))
            return false;
        
        // Refresh control task proxy to get to know the new receiver ports.
        if(!createProxy())
            return false;

        receiverConnected = true;
        return true;
    }

    /**
     * Connects the sender output port to the receiver sender port.
     * This requires valid ports (on the sender and the receiver) and a valid proxy.
     */
    bool connectPorts()
    {
        connected = false;
        if(!portsCreated || !proxyCreated || !receiverConnected)
            return false;

        // Get pointer to the input port of the receiver.
        RTT::InputPortInterface* remoteinputport = NULL;
        remoteinputport = (RTT::InputPortInterface*)controlTaskProxy->ports()->
                getPort(outputPortName);

        if(remoteinputport == NULL)
            return false;
    
        // Connect the output port of the sender to the input port of the receiver.
        // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
        // true=pull(problem here) false=push)
        if(!outputPort->connectTo(*remoteinputport, 
                RTT::ConnPolicy::buffer(20, RTT::ConnPolicy::LOCKED, false, false)))
            return false;

        connected = true;
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
    bool portsCreated;
    bool proxyCreated;
    bool receiverConnected; // Ports are created and output port is connected.
    bool connected;
};
} // namespace root
#endif
