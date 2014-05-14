#include "MessageTransportTask.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <fipa_services/MessageTransport.hpp>
#include <fipa_services/DistributedServiceDirectory.hpp>
#include <fipa_services/transports/Transport.hpp>
#include <fipa_services/transports/tcp/SocketTransport.hpp>
#include <fipa_services/transports/udt/UDTTransport.hpp>

#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>

#include <uuid/uuid.h>

namespace rc = RTT::corba;

using namespace RTT;

namespace fipa_services
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
MessageTransportTask::MessageTransportTask(std::string const& name)
    : MessageTransportTaskBase(name)
    , mMessageTransport(0)
    , mDistributedServiceDirectory(0)
    , mUDTNode(0)
{}

MessageTransportTask::MessageTransportTask(std::string const& name, RTT::ExecutionEngine* engine)
    : MessageTransportTaskBase(name, engine)
    , mMessageTransport(0)
    , mDistributedServiceDirectory(0)
    , mUDTNode(0)
{}

MessageTransportTask::~MessageTransportTask()
{}

////////////////////////////////HOOKS///////////////////////////////
bool MessageTransportTask::configureHook()
{
    if(!MessageTransportTaskBase::configureHook())
    {
        return false;
    }

    mDistributedServiceDirectory = new fipa::services::DistributedServiceDirectory();

    // Create internal message transport service

    // Make sure the MTS name is unique for each running instance
    uuid_t uuid;
    uuid_generate(uuid);
    char mtsUID[512];
    uuid_unparse(uuid, mtsUID);

    fipa::acl::AgentID agentName(this->getName() + "-" + std::string(mtsUID));
    mMessageTransport = new fipa::services::message_transport::MessageTransport(agentName);
    
    
    mUDTTransport = new fipa::services::Transport(getName(), mDistributedServiceDirectory,
                                                  new fipa::services::ServiceLocation(mUDTNode->getAddress(mInterface).toString(), this->getModelName()));
    
    // register the default transport
    mMessageTransport->registerTransport("default-udt-transport", 
                                         boost::bind(&fipa::services::Transport::deliverOrForwardLetterViaUDT, mUDTTransport,_1));
    
    // TODO it should be configurable, which transports to use
    mSocketTransport = new fipa::services::message_transport::SocketTransport(mMessageTransport, mDistributedServiceDirectory);
    // register socket transport
    mMessageTransport->registerTransport("socket-transport", 
                                         boost::bind(&fipa::services::message_transport::SocketTransport::deliverForwardLetter, mSocketTransport, _1));

    mUDTNode = new fipa::services::udt::Node();
    mUDTNode->listen();
    mInterface = _nic.get();
    
    mSocketServiceLocation = new fipa::services::ServiceLocation(mSocketTransport->getAddress(mInterface).toString(), "fipa::services::message_transport::SocketTransport");

    return true;
}

void MessageTransportTask::errorHook()
{
    MessageTransportTaskBase::errorHook();
    RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "'" << " : Entering error state." << RTT::endlog();
}

bool MessageTransportTask::startHook()
{
    if(!MessageTransportTaskBase::startHook())
        return false;

    return true;
}

void MessageTransportTask::updateHook()
{
    using namespace fipa::acl;

    fipa::SerializedLetter serializedLetter;
    while( _letters.read(serializedLetter) == RTT::NewData)
    {
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : received new letter of size '" << serializedLetter.getVector().size() << "'" << RTT::endlog();

        fipa::acl::Letter letter = serializedLetter.deserialize();
        fipa::acl::ACLBaseEnvelope be = letter.flattened();
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : intended receivers: " << be.getIntendedReceivers() << ", content: " << letter.getACLMessage().getContent() << RTT::endlog();
        mMessageTransport->handle(letter);
    }

    // Accept new connections
    mUDTNode->accept();

    // Handle MTS UDT data transfer
    mUDTNode->update();
    while(mUDTNode->hasLetter())
    {
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : node received letter " << RTT::endlog();
        fipa::acl::Letter letter = mUDTNode->nextLetter();
        mMessageTransport->handle(letter);
    }
}

void MessageTransportTask::stopHook()
{
    MessageTransportTaskBase::stopHook();
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////

void MessageTransportTask::cleanupHook()
{
    MessageTransportTaskBase::cleanupHook();

    delete mUDTNode;
    mUDTNode = NULL;
    
    delete mSocketServiceLocation;
    mSocketServiceLocation = NULL;

    delete mDistributedServiceDirectory;
    mDistributedServiceDirectory = NULL;

    delete mMessageTransport;
    mMessageTransport = NULL;
    
    delete mSocketTransport;
    mSocketTransport = NULL;
}


////////////////////////////////RPC-METHODS//////////////////////////
std::vector<std::string> MessageTransportTask::getReceivers()
{
    ReceiverPorts::const_iterator cit = mReceivers.begin();
    std::vector<std::string> receivers;
    for(; cit != mReceivers.end(); ++cit)
    {
        receivers.push_back(cit->first);
    }
    return receivers;
}

bool MessageTransportTask::addReceiver(::std::string const & receiver, bool is_local)
{
    std::string output_port_name(receiver);

    RTT::base::PortInterface *pi = ports()->getPort(receiver);
    if(pi) // we are already having a connection of the given name
    {
        RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : connection " << receiver << " is already registered" << RTT::endlog();
        // Since the connection already exists, everything is good?!
        // so could be true here as well
        return false;
    }

    RTT::base::PortInterface* port = _letters.antiClone();
    port->setName(receiver);

    RTT::base::OutputPortInterface *out_port = dynamic_cast<RTT::base::OutputPortInterface*>(port);
    if(!out_port)
    {
        RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : could not cast anticlone to outputport" << RTT::endlog();
        return false;
    }

    bool success = addReceiverPort(out_port, receiver);
    if(success && is_local)
    {
        fipa::services::ServiceLocator locator;
        // foreach transport ins transports: get servicelocation
        locator.addLocation(*(mUDTTransport->getServiceLocationP()));
        // FIXME transport.getServiceLocation()
        locator.addLocation(*mSocketServiceLocation);

        fipa::services::ServiceDirectoryEntry client(receiver, "mts_client", locator, "Message client of " + getName());
        mDistributedServiceDirectory->registerService(client);
    }

    return success;
}


bool MessageTransportTask::removeReceiver(::std::string const & receiver)
{
    using namespace fipa::services;
    if(removeReceiverPort(receiver))
    {
        try {
            mDistributedServiceDirectory->deregisterService(receiver, ServiceDirectoryEntry::NAME);
            return true;
        } catch(const NotFound& e)
        {
            return false;
        }
    }

    return false;
}

bool MessageTransportTask::addReceiverPort(RTT::base::OutputPortInterface* outputPort, const std::string& receiver)
{
    boost::unique_lock<boost::shared_mutex> lock(mServiceChangeMutex);

    ports()->addPort(outputPort->getName(), *outputPort);
    mReceivers[receiver] = outputPort;
    RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : Receiver port '" << receiver << "' added" << RTT::endlog();
    return true;
}

bool MessageTransportTask::removeReceiverPort(const std::string& receiver)
{
    boost::unique_lock<boost::shared_mutex> lock(mServiceChangeMutex);

    ReceiverPorts::iterator it = mReceivers.find(receiver);
    if(it != mReceivers.end())
    {
        ports()->removePort(it->second->getName());
        delete it->second;
        mReceivers.erase(it);

        return true;
    }

    RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : No output port named '" << receiver << "' registered" << RTT::endlog();

    return false;
}

void MessageTransportTask::serviceAdded(servicediscovery::avahi::ServiceEvent se)
{
    std::string serviceName = se.getServiceConfiguration().getName();
    std::string serviceTaskModel = se.getServiceConfiguration().getDescription("TASK_MODEL");
    std::string ior = se.getServiceConfiguration().getDescription("IOR");
    if(serviceTaskModel == this->getModelName())
    {
        connectToMTS(serviceName, ior);
    }
}

void MessageTransportTask::connectToMTS(const std::string& serviceName, const std::string& ior)
{
}

void MessageTransportTask::serviceRemoved(servicediscovery::avahi::ServiceEvent se)
{
    std::string serviceName = se.getServiceConfiguration().getName();
    std::string serviceTaskModel = se.getServiceConfiguration().getDescription("TASK_MODEL");
    if(serviceTaskModel == this->getModelName())
    {
        if(serviceName != getName())
        {
            removeReceiver(serviceName);
        }
    }
}

void MessageTransportTask::addSocketTransport()
{
    // TODO remove
}

} // namespace fipa_services
