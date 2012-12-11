#include "corba_connection.h"

#include <rtt/TaskContext.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>
#include <rtt/internal/RemoteOperationCaller.hpp>
#include <boost/cstdint.hpp>

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
CorbaConnection::CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
        std::string receiver_ior, uint32_t buffer_size) : 
        ConnectionInterface(),
        receiverIOR(receiver_ior),
        bufferSize(buffer_size),
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
    try {
        log(RTT::Debug) << "CorbaConnection::connect() " << RTT::endlog();

        if(connected)
        {
            log(RTT::Debug) << "CorbaConnection::connect() - is already connected. " << RTT::endlog();
            return true;
        }

        if(!createProxy())
        {
            throw ConnectionException("Sender proxy could not be created.");
        }

        log(RTT::Debug) << "CorbaConnection::connect() - not yet connected. " << RTT::endlog();

        if(!createPorts())
        {
            throw ConnectionException("Sender ports could not be created.");
        }

        if(!createConnectPortsOnReceiver<bool(std::string const&, std::string const&, boost::int32_t)>
                ("rpcCreateConnectPorts"))
        {
            throw ConnectionException("Receiver ports could not be created/connected.");
        }

        if(!connectPorts())
        {
            throw ConnectionException("Sender ports could not be connected.");
        }

        connected = true;
        return true;
    } catch(CORBA::COMM_FAILURE&)
    {
        log(RTT::Error) << "CorbaConnection::connect() - CORBA::COMM_FAILURE" << RTT::endlog();
        throw ConnectionException("CORBA::COMM_FAILURE");
    } catch(CORBA::TRANSIENT&)
    {
        log(RTT::Error) << "CorbaConnection::connect() - CORBA::COMM_FAILURE" << RTT::endlog();
        throw ConnectionException("CORBA::TRANSIENT");
    } catch(CORBA::INV_OBJREF&)
    {
        log(RTT::Error) << "CorbaConnection::connect() - CORBA::INV_OBJREF" << RTT::endlog();
        throw ConnectionException("CORBA::INV_OBJREF");
    } catch(ConnectionException& e)
    {
        // forward only
        throw;
    } catch(...)
    {
        log(RTT::Error) << "CorbaConnection::connect() - unknown error" << RTT::endlog();
        throw ConnectionException("Unknown error during connect");
    }
}

bool CorbaConnection::connectLocal() //virtual
{
    try {
        if(connected)
            return true;

        if(!createProxy())
        {
            throw ConnectionException("Sender proxy could not be created.");
        }

        if(!createPorts())
        {
            throw ConnectionException("Sender ports could not be created.");
        }

        if(!connectPorts())
        {
            throw ConnectionException("Sender ports could not be connected.");
        }
        connected = true;
        return true;

    } catch(CORBA::COMM_FAILURE&)
    {
        log(RTT::Error) << "CorbaConnection::connectLocal() - CORBA::COMM_FAILURE" << RTT::endlog();
        throw ConnectionException("COBRA::COMM_FAILURE");
    } catch(CORBA::TRANSIENT&)
    {
        log(RTT::Error) << "CorbaConnection::connectLocal() - CORBA::COMM_FAILURE" << RTT::endlog();
        throw ConnectionException("COBRA::TRANSIENT");
    } catch(CORBA::INV_OBJREF&)
    {
        log(RTT::Error) << "CorbaConnection::connect() - CORBA::INV_OBJREF" << RTT::endlog();
        throw ConnectionException("CORBA::INV_OBJREF");
    } catch(ConnectionException& e)
    {
        throw;
    } catch(...)
    {
        log(RTT::Error) << "CorbaConnection::connect() - unknown error" << RTT::endlog();
        throw ConnectionException("Unknown error during connect");
    }
}

bool CorbaConnection::disconnect()
{ 
    try {
        log(RTT::Debug) << "CorbaConnection::disconnect() " << RTT::endlog();

        if(!controlTaskProxy)
        {
            RTT::log(RTT::Debug) << "CorbaConnection: no control task proxy set -- disconnect() defaults to returning false" << RTT::endlog();
            return false;
        }

        if(taskContextSender->hasPeer(controlTaskProxy->getName()))
        {
            RTT::log(RTT::Debug) << "CorbaConnection: known peer '" << controlTaskProxy->getName() << "' will be removed" << RTT::endlog();
            taskContextSender->removePeer(controlTaskProxy->getName());
            delete controlTaskProxy;
            controlTaskProxy = NULL;
            
        } else {
            RTT::log(RTT::Debug) << "CorbaConnection: peer '" << controlTaskProxy->getName() << "' is unknown" << RTT::endlog();
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
    } catch(CORBA::COMM_FAILURE&)
    {
        log(RTT::Error) << "CorbaConnection::disconnect() - CORBA::COMM_FAILURE" << RTT::endlog();
        return false;
    } catch(CORBA::TRANSIENT&)
    {
        log(RTT::Error) << "CorbaConnection::disconnect() - CORBA::COMM_FAILURE" << RTT::endlog();
        return false;
    } catch(...)
    {
        log(RTT::Error) << "CorbaConnection::disconnect() - unknown exception" << RTT::endlog();
        return false;
    }
}

std::string CorbaConnection::read() //virtual
{
    if(!connected)
    {
        log(RTT::Info) << "CorbaConnection: data can not be read, no connection available. " << 
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

    taskContextSender->ports()->addEventPort(inputPortName,*inputPort);
    log(RTT::Info) << "CorbaConnection: created event port: " << inputPortName << RTT::endlog();
	
    taskContextSender->ports()->addPort(outputPortName, *outputPort);
    log(RTT::Info) << "CorbaConnection: created port: " << outputPortName << RTT::endlog();
 
    portsCreated = true;
    return true;
}

bool CorbaConnection::createProxy()
{
    proxyCreated = false;

    // overwrite the last peer connection
    if(taskContextSender->hasPeer(receiverName))
    {
        RTT::log(RTT::Debug) << "CorbaConnection: known peer '" << receiverName << "' will be removed" << RTT::endlog();
        taskContextSender->removePeer(receiverName);
        if(controlTaskProxy)
        {
            delete controlTaskProxy;
            controlTaskProxy = NULL;
        }
    } else {
        RTT::log(RTT::Debug) << "CorbaConnection: peer '" << receiverName << "' is unknown" << RTT::endlog();
    }

    // Create Control Task Proxy.
    RTT::corba::TaskContextProxy::InitOrb(0, 0);
    try {
        controlTaskProxy = RTT::corba::TaskContextProxy::
                Create(receiverIOR, receiverIOR.substr(0,3) == "IOR");
        RTT::log(RTT::Info) << "CorbaConnection: created proxy for IOR: '" << receiverIOR << "'" << RTT::endlog();
    } catch(CORBA::OBJECT_NOT_EXIST& e)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for ior '" << receiverIOR << "' does not exist." << RTT::endlog();
        return false;
    } catch(CORBA::INV_OBJREF& e)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for  ior '" << receiverIOR << "' invalid: " << RTT::endlog();
        return false;
    } catch(...)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for  ior '" << receiverIOR << "' (probably) invalid." << RTT::endlog();
        return false;
    }

    // Creating a one-directional connection from task_context to the peer. 
    if( !taskContextSender->addPeer(controlTaskProxy) )
    {
        RTT::log(RTT::Warning) << "CorbaConnection: adding peer failed" << RTT::endlog();
        return false;
    }
    
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
    {
	log(RTT::Info) << "Receiver port not ready. " << RTT::endlog();
        return false;
    }
 
    // Create receiver connection.
    if(!create_receiver_ports.call(senderName, senderIOR, bufferSize))
    {
	log(RTT::Info) << "Receiver port creation failed. " << RTT::endlog();
        return false;
    }
    
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
    {
	log(RTT::Info) << "Ports/Proxy not created" << RTT::endlog();
        return false;
    }

    // Get pointer to the input port of the receiver.
    RTT::base::InputPortInterface* remoteinputport = NULL;
    remoteinputport = (RTT::base::InputPortInterface*)controlTaskProxy->ports()->
            getPort(outputPortName);
    log(RTT::Info) << "Remote output port is: " << outputPortName << RTT::endlog();

    if(remoteinputport == NULL)
    {
	log(RTT::Info) << "Remote output port" << RTT::endlog();
        return false;
    }

    // Connect the output port of the sender to the input port of the receiver.
    // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
    // true=pull(problem here) false=push)
    if(!outputPort->connectTo(remoteinputport, 
            RTT::ConnPolicy::buffer(bufferSize, RTT::ConnPolicy::LOCKED, false, false)))
    {
	log(RTT::Info) << "Local output port could not be connected to remote input port" << RTT::endlog();
        return false;
    }

    portsConnected = true;
    return true;
}
} // namespace root
