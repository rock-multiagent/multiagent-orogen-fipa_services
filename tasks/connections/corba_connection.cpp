#include "corba_connection.h"

#include <rtt/TaskContext.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>
#include <rtt/internal/RemoteOperationCaller.hpp>
#include <boost/cstdint.hpp>

namespace mts
{
    std::string FIPA_CORBA_TRANSPORT_SERVICE = "FIPA_CORBA_TRANSPORT_SERVICE";
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
CorbaConnection::CorbaConnection(RTT::TaskContext* sender, std::string receiver, 
        std::string receiver_ior, uint32_t buffer_size) : 
        ConnectionInterface(),
        mReceiverIOR(receiver_ior),
        mBufferSize(buffer_size),
        mTaskContextSender(sender),
        mControlTaskProxy(NULL),
        mInputPort(NULL),
        mOutputPort(NULL),
        mPortsCreated(false),
        mProxyCreated(false),
        mReceiverConnected(false),
        mPortsConnected(false)
{
    mReceiverName = receiver;
    mConnected = false;
    mSenderName = sender->getName();
    mSenderIOR = RTT::corba::TaskContextServer::getIOR(sender);
    mInputPortName = mReceiverName + "->" + mSenderName + "_port";    
    mOutputPortName = mSenderName + "->" + mReceiverName + "_port";
}
            
CorbaConnection::~CorbaConnection()
{
    disconnect();
}

bool CorbaConnection::connect() //virtual
{ 
    try {
        log(RTT::Debug) << "CorbaConnection::connect() " << RTT::endlog();

        if(mConnected)
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

        mConnected = true;
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
        if(mConnected)
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
        mConnected = true;
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

        if(!mControlTaskProxy)
        {
            RTT::log(RTT::Debug) << "CorbaConnection: no control task proxy set -- disconnect() defaults to returning false" << RTT::endlog();
            return false;
        }

        if(mTaskContextSender->hasPeer(mControlTaskProxy->getName()))
        {
            RTT::log(RTT::Debug) << "CorbaConnection: known peer '" << mControlTaskProxy->getName() << "' will be removed" << RTT::endlog();
            mTaskContextSender->removePeer(mControlTaskProxy->getName());
            delete mControlTaskProxy;
            mControlTaskProxy = NULL;
            
        } else {
            RTT::log(RTT::Debug) << "CorbaConnection: peer '" << mControlTaskProxy->getName() << "' is unknown" << RTT::endlog();
        }

        if(mInputPort)
        {
            mTaskContextSender->ports()->removePort(mInputPortName);
            delete mInputPort;
            mInputPort = NULL;
        }
        if(mOutputPort)
        {
            mTaskContextSender->ports()->removePort(mOutputPortName);
            delete mOutputPort;
            mOutputPort = NULL;
        }
        
        mConnected = false;
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

fipa::acl::Letter CorbaConnection::read() //virtual
{
    if(!mConnected)
    {
        log(RTT::Info) << "CorbaConnection: data can not be read, no connection available. " << RTT::endlog();
        throw ConnectionException("Cannot read data since no connection is available");
    }
    fipa::SerializedLetter serializedLetter;
    if(!mInputPort->read(serializedLetter))
    {
        throw ConnectionException("Error reading message.");
    }
    return serializedLetter.deserialize();
}

bool CorbaConnection::send(fipa::acl::Letter const& data, fipa::acl::representation::Type representation) //virtual
{
    if(!mConnected)
    {
        log(RTT::Info) << "Data can not be sent, no connection available. " << 
            RTT::endlog();
        return false;
    }
    // string to char-vector conversion to handle zeros within the string. 
    fipa::SerializedLetter serializedLetter(data, representation);
    mOutputPort->write(serializedLetter);
    return true;
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
bool CorbaConnection::createPorts()
{
    // Do nothing if the ports have already been created.
    if(mPortsCreated)
        return true;

    mInputPort = new RTT::InputPort<fipa::SerializedLetter>(mInputPortName);
    mOutputPort = new RTT::OutputPort<fipa::SerializedLetter>(mOutputPortName);

    mTaskContextSender->ports()->addEventPort(mInputPortName,*mInputPort);
    log(RTT::Info) << "CorbaConnection: created event port: " << mInputPortName << RTT::endlog();
	
    mTaskContextSender->ports()->addPort(mOutputPortName, *mOutputPort);
    log(RTT::Info) << "CorbaConnection: created port: " << mOutputPortName << RTT::endlog();
 
    mPortsCreated = true;
    return true;
}

bool CorbaConnection::createProxy()
{
    mProxyCreated = false;

    // overwrite the last peer connection
    if(mTaskContextSender->hasPeer(mReceiverName))
    {
        RTT::log(RTT::Debug) << "CorbaConnection: known peer '" << mReceiverName << "' will be removed" << RTT::endlog();
        mTaskContextSender->removePeer(mReceiverName);
        if(mControlTaskProxy)
        {
            delete mControlTaskProxy;
            mControlTaskProxy = NULL;
        }
    } else {
        RTT::log(RTT::Debug) << "CorbaConnection: peer '" << mReceiverName << "' is unknown" << RTT::endlog();
    }

    // Create Control Task Proxy.
    RTT::corba::TaskContextProxy::InitOrb(0, 0);
    try {
        mControlTaskProxy = RTT::corba::TaskContextProxy::
                Create(mReceiverIOR, mReceiverIOR.substr(0,3) == "IOR");
        RTT::log(RTT::Info) << "CorbaConnection: created proxy for IOR: '" << mReceiverIOR << "'" << RTT::endlog();
    } catch(CORBA::OBJECT_NOT_EXIST& e)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for ior '" << mReceiverIOR << "' does not exist." << RTT::endlog();
        return false;
    } catch(CORBA::INV_OBJREF& e)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for  ior '" << mReceiverIOR << "' invalid: " << RTT::endlog();
        return false;
    } catch(...)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for  ior '" << mReceiverIOR << "' (probably) invalid." << RTT::endlog();
        return false;
    }

    if( mControlTaskProxy->provides()->hasService(::mts::FIPA_CORBA_TRANSPORT_SERVICE))
    {
        std::string errorMsg = "Remote service does not match message transport service type: '";
        RTT::log(RTT::Error) << errorMsg << RTT::endlog();
        throw InvalidService(errorMsg);
    }

    // Creating a one-directional connection from task_context to the peer. 
    if( !mTaskContextSender->addPeer(mControlTaskProxy) )
    {
        RTT::log(RTT::Warning) << "CorbaConnection: adding peer failed" << RTT::endlog();
        return false;
    }
    
    mProxyCreated = true;
    return true;
}

template<class Signature>
bool CorbaConnection::createConnectPortsOnReceiver(std::string function_name)
{
    mReceiverConnected = false;
    if(!mPortsCreated || !mProxyCreated)
        return false;
    
	// WARNING: no typesafety any more
	if(!mControlTaskProxy->ready())
		return false;

    RTT::OperationCaller<Signature> create_receiver_ports = mControlTaskProxy->getOperation(function_name);

    // Receiver function is ready?
    if(!create_receiver_ports.ready())
    {
	log(RTT::Info) << "Receiver port not ready. " << RTT::endlog();
        return false;
    }
 
    // Create receiver connection.
    if(!create_receiver_ports.call(mSenderName, mSenderIOR, mBufferSize))
    {
	log(RTT::Info) << "Receiver port creation failed. " << RTT::endlog();
        return false;
    }
    
    // Refresh control task proxy to get to know the new receiver ports.
    if(!createProxy())
        return false;
	

    mReceiverConnected = true;
    return true;
}

bool CorbaConnection::connectPorts()
{
    mConnected = false;
    if(!mPortsCreated || !mProxyCreated)
    {
	log(RTT::Info) << "Ports/Proxy not created" << RTT::endlog();
        return false;
    }

    // Get pointer to the input port of the receiver.
    RTT::base::InputPortInterface* remoteinputport = NULL;
    remoteinputport = dynamic_cast<RTT::base::InputPortInterface*>( mControlTaskProxy->ports()->
            getPort(mOutputPortName) );
    log(RTT::Info) << "Remote output port is: " << mOutputPortName << RTT::endlog();

    if(remoteinputport == NULL)
    {
	log(RTT::Info) << "Remote output port" << RTT::endlog();
        return false;
    }

    // Connect the output port of the sender to the input port of the receiver.
    // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
    // true=pull(problem here) false=push)
    if(!mOutputPort->connectTo(remoteinputport, 
            RTT::ConnPolicy::buffer(mBufferSize, RTT::ConnPolicy::LOCKED, false, false)))
    {
	log(RTT::Info) << "Local output port could not be connected to remote input port" << RTT::endlog();
        return false;
    }

    mPortsConnected = true;
    return true;
}

void CorbaConnection::updateAgentList(const std::vector<std::string>& agents)
{
    assert(mControlTaskProxy);

    RTT::OperationCaller<bool(const std::string&, const std::vector<std::string>& )> updateAgentList = mControlTaskProxy->getOperation("updateAgentList");

    // Receiver function is ready?
    if(!updateAgentList.ready())
    {
        RTT::log(RTT::Info) << "Receiver: updateAgentList not ready. " << RTT::endlog();
        return;
    }
 
    // Create receiver connection.
    if(!updateAgentList.call(mSenderName, agents))
    {
        RTT::log(RTT::Warning) << "Receiver: updateAgentList failed on: '" << mReceiverName << "'" << ". Called by '" << mSenderName << "' with agent list of size: '" << agents.size() << "'" << RTT::endlog();
    }
}


} // namespace mts
