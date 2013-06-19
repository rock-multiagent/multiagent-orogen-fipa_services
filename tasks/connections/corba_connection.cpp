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
    mInputPortName = "letters";    
    mOutputPortName = mReceiverName; 
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

//        log(RTT::Debug) << "CorbaConnection::connect() - not yet connected. " << RTT::endlog();
//        if(!connectPorts())
//        {
//            throw ConnectionException("Sender ports could not be connected.");
//        } else {
//            log(RTT::Debug) << "CorbaConnection::connect() -- connected. " << RTT::endlog();
//        }

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

    } catch(CORBA::SystemException& ex) 
    {
        log(RTT::Error) << "Caught CORBA::SystemException " << ex._name() << RTT::endlog();
        throw ConnectionException("CORBA::SystemException");
    } 
    catch(CORBA::Exception& ex) 
    {
        log(RTT::Error) << "Caught CORBA::Exception: " << ex._name() << RTT::endlog();
        throw ConnectionException("CORBA::Exception");
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

bool CorbaConnection::disconnect()
{ 
    try {
        log(RTT::Debug) << "CorbaConnection::disconnect() " << RTT::endlog();

        if(!mControlTaskProxy)
        {
            RTT::log(RTT::Debug) << "CorbaConnection: no control task proxy set -- disconnect() defaults to returning false" << RTT::endlog();
            return false;
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
    } catch(CORBA::SystemException& ex) 
    {
        log(RTT::Error) << "Caught CORBA::SystemException " << ex._name() << RTT::endlog();
        return false;
    } 
    catch(CORBA::Exception& ex) 
    {
        log(RTT::Error) << "Caught CORBA::Exception: " << ex._name() << RTT::endlog();
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
bool CorbaConnection::createProxy()
{
    mProxyCreated = false;

    // Create Control Task Proxy.
    RTT::corba::TaskContextProxy::InitOrb(0, 0);
    //RTT::corba::ApplicationServer::InitOrb(0,0);
    try {
//        RTT::log(RTT::Error) << "CorbaConnection: Creating proxy" << RTT::endlog();
//
//        if(CORBA::is_nil(RTT::corba::ApplicationServer::orb))
//            throw std::runtime_error("Corba is not initialized");
//
//        // Use the ior to create the task object reference,
//        CORBA::Object_var task_object;
//        try
//        {
//            task_object = RTT::corba::ApplicationServer::orb->string_to_object ( mReceiverIOR.c_str() );
//        }
//        catch(CORBA::SystemException &e)
//        {
//            throw std::runtime_error("given IOR " + mReceiverIOR + " is not valid");
//        }
//            
//        // Then check we can actually access it
//        //RTT::corba::CTaskContext_var mRemoteTask;
//        // Now downcast the object reference to the appropriate type
//        mRemoteTask = RTT::corba::CTaskContext::_narrow (task_object.in());
//        
//        if(CORBA::is_nil( mRemoteTask ))
//            throw std::runtime_error("cannot narrow task context.");
//
//        bool forceRemote = true;
//        mControlTaskProxy = RTT::corba::TaskContextProxy::
//            Create(mRemoteTask, forceRemote);

        // With multiple host starting up simulatenously, this call leads to a segfault
        // in CServer->getName(), i.e. the corba implementation. 
        // Trying to workaround by going through the application server first to retrieve reference
        mControlTaskProxy = RTT::corba::TaskContextProxy::
         Create(mReceiverName); //mReceiverIOR, mReceiverIOR.substr(0,3) == "IOR");

        RTT::log(RTT::Info) << "CorbaConnection: created proxy for IOR: '" << mReceiverIOR << "'" << RTT::endlog();
    } catch(CORBA::OBJECT_NOT_EXIST& e)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for ior '" << mReceiverIOR << "' does not exist." << RTT::endlog();
        return false;
    } catch(CORBA::INV_OBJREF& e)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for  ior '" << mReceiverIOR << "' invalid: " << RTT::endlog();
        return false;
    } /*catch(...)
    {
        RTT::log(RTT::Warning) << "CorbaConnection: creating proxy failed: object for  ior '" << mReceiverIOR << "' (probably) invalid." << RTT::endlog();
        return false;
    }*/

    if(mControlTaskProxy->ready())
    {
        if(!mControlTaskProxy->provides()->hasService(::mts::FIPA_CORBA_TRANSPORT_SERVICE))
        {
            std::string errorMsg = "Remote service does not match message transport service type: '" + ::mts::FIPA_CORBA_TRANSPORT_SERVICE + "'";
            RTT::log(RTT::Error) << errorMsg << RTT::endlog();
            //throw InvalidService(errorMsg);
        }
    }
    
    mProxyCreated = true;
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

    mOutputPort = dynamic_cast<RTT::OutputPort<fipa::SerializedLetter>* >(mTaskContextSender->ports()->getPort(mOutputPortName) );
    assert(mOutputPort);

    // Get pointer to the input port of the receiver.
    RTT::base::InputPortInterface* remoteinputport = NULL;
    remoteinputport = dynamic_cast<RTT::base::InputPortInterface*>( mControlTaskProxy->ports()->
            getPort(mInputPortName) );

    log(RTT::Info) << "Remote input port is: " << mInputPortName << RTT::endlog();

    if(remoteinputport == NULL)
    {
        log(RTT::Info) << "Remote input port is not available" << RTT::endlog();
        return false;
    }

    // Connect the output port of the sender to the input port of the receiver.
    // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
    // true=pull(problem here) false=push)
    if(!mOutputPort->createConnection(*remoteinputport, 
            RTT::ConnPolicy::buffer(mBufferSize, RTT::ConnPolicy::LOCKED, false, false)))
    {
        log(RTT::Info) << "Local output port '" << mOutputPort->getName() << "' could not be connected to remote input port '" << remoteinputport->getName() << "' of mts '" << mReceiverName << "'" << RTT::endlog();
        return false;
    }

    mPortsConnected = true;
    return true;
}

void CorbaConnection::updateAgentList(const std::vector<std::string>& agents)
{
    if(!mControlTaskProxy)
    {
        createProxy();
    }
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
