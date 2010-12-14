#include "corba_connection.h"

#include <rtt/TaskContext.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>
#include <rtt/internal/RemoteOperationCaller.hpp>

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
CorbaConnection::CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
        std::string receiver_ior) : 
        ConnectionInterface(),
        receiverIOR(receiver_ior),
        taskContextSender(sender),
        controlTaskProxy(NULL),
        inputPort(NULL),
        outputPort(NULL),
        portsCreated(false),
        proxyCreated(false),
        receiverConnected(false),
        portsConnected(false)
{
    receiverName = receiver;
    connected = false;
    senderName = sender->getName();
    senderIOR = RTT::corba::TaskContextServer::getIOR(sender);
    inputPortName = receiverName + "->" + senderName + "_port";    
    outputPortName = senderName + "->" + receiverName + "_port";
}
            
CorbaConnection::~CorbaConnection()
{
    disconnect();
}

bool CorbaConnection::connect() //virtual
{
    if(connected)
        return true;

    if(!createPorts())
    {
        throw ConnectionException("Sender ports could not be created.");
        return false;
    }

    if(!createProxy())
    {
        throw ConnectionException("Sender proxy could not be created.");
        return false;
    }

    if(!createConnectPortsOnReceiver<bool(std::string const&, std::string const&)>
            ("rpcCreateConnectPorts"))
    {
        throw ConnectionException("Receiver ports could not be created/connected.");
        return false;
    }

    if(!connectPorts())
    {
        throw ConnectionException("Sender ports could not be connected.");
        return false;
    }

    connected = true;
    return true;
}

bool CorbaConnection::connectLocal() //virtual
{
    if(connected)
        return true;

    if(!createPorts())
    {
        throw ConnectionException("Sender ports could not be created.");
        return false;
    }

    if(!createProxy())
    {
        throw ConnectionException("Sender proxy could not be created.");
        return false;
    }

    if(!connectPorts())
    {
        throw ConnectionException("Sender ports could not be connected.");
        return false;
    }
    connected = true;
    return true;
}

bool CorbaConnection::disconnect()
{
    if(controlTaskProxy)
    {
        taskContextSender->removePeer(receiverIOR);
        delete controlTaskProxy;
        controlTaskProxy = NULL;
    }
    if(inputPort)
    {
        taskContextSender->ports()->removePort(inputPortName);
        delete inputPort;
        inputPort = NULL;
    }
    if(outputPort)
    {
        taskContextSender->ports()->removePort(outputPortName);
        delete outputPort;
        outputPort = NULL;
    }
    
    connected = false;
    return true;
}

std::string CorbaConnection::read() //virtual
{
    if(!connected)
    {
        log(RTT::Info) << "Data can not be read, no connection available. " << 
            RTT::endlog();
        return false;
    }
    fipa::BitefficientMessage msg;
    if(!inputPort->read(msg))
    {
        throw ConnectionException("Error reading message.");
    }
    return msg.toString();
}

bool CorbaConnection::send(std::string const& data) //virtual
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

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
bool CorbaConnection::createPorts()
{
    // Do nothing if the ports have already been created.
    if(portsCreated)
        return true;

    inputPort = new RTT::InputPort<fipa::BitefficientMessage>(inputPortName);
    outputPort = new RTT::OutputPort<fipa::BitefficientMessage>(outputPortName);

	std::string inputPortName = "Input port to "+receiverName;
    taskContextSender->ports()->addEventPort(inputPortName,*inputPort);
	
	std::string outputPortName = "Output port to "+receiverName;
    taskContextSender->ports()->addPort(outputPortName, *outputPort);
 
    portsCreated = true;
    return true;
}

bool CorbaConnection::createProxy()
{
    proxyCreated = false;
    if(controlTaskProxy != NULL)
    {
        taskContextSender->removePeer(receiverIOR);
        delete controlTaskProxy;
        controlTaskProxy = NULL;
    }

    // Create Control Task Proxy.
    RTT::corba::TaskContextProxy::InitOrb(0, 0);
    controlTaskProxy = RTT::corba::TaskContextProxy::
            Create(receiverIOR, receiverIOR.substr(0,3) == "IOR");

    // Creating a one-directional connection from task_context to the peer. 
    if(!taskContextSender->addPeer(controlTaskProxy))
        return false;
    
    proxyCreated = true;
    return true;
}

template<class Signature>
bool CorbaConnection::createConnectPortsOnReceiver(std::string function_name)
{
    receiverConnected = false;
    if(!portsCreated || !proxyCreated)
        return false;
    
	// WARNING: no typesafety any more
	if(!controlTaskProxy->ready())
		return false;

    RTT::OperationCaller<Signature> create_receiver_ports = controlTaskProxy->getOperation(function_name);

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

bool CorbaConnection::connectPorts()
{
    connected = false;
    if(!portsCreated || !proxyCreated)
        return false;

    // Get pointer to the input port of the receiver.
    RTT::base::InputPortInterface* remoteinputport = NULL;
    remoteinputport = (RTT::base::InputPortInterface*)controlTaskProxy->ports()->
            getPort(outputPortName);

    if(remoteinputport == NULL)
        return false;

    // Connect the output port of the sender to the input port of the receiver.
    // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
    // true=pull(problem here) false=push)
    if(!outputPort->connectTo(remoteinputport, 
            RTT::ConnPolicy::buffer(20, RTT::ConnPolicy::LOCKED, false, false)))
        return false;

    portsConnected = true;
    return true;
}
} // namespace root
