#include "MessageTransportTask.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/assign/list_of.hpp>

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
    
    
    // TODO it should be configurable, which transports to use
    // Initalize UDT
    mUDTNode = new fipa::services::udt::Node();
    mUDTNode->listen();
    mInterface = _nic.get();
    // Initialize TCP
    fipa::services::tcp::SocketTransport::startListening(mMessageTransport);
    
    // Register the local delivery
    mMessageTransport->registerTransport("local-delivery", 
                                         boost::bind(&fipa_services::MessageTransportTask::deliverLetterLocally, this,_1));
    
    mDefaultTransport = new fipa::services::Transport(getName(), mDistributedServiceDirectory,
                                                  boost::assign::list_of
                                                  (fipa::services::ServiceLocation(mUDTNode->getAddress(mInterface).toString(), "fipa::services::udt::UDTTransport"))
                                                  (fipa::services::ServiceLocation(fipa::services::tcp::SocketTransport::getAddress(mInterface).toString(), "fipa::services::tcp::SocketTransport")));
    // register the default transport
    mMessageTransport->registerTransport("default-transport", 
                                         boost::bind(&fipa::services::Transport::deliverOrForwardLetter, mDefaultTransport,_1));

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

    // Also explicitly register all services, if we already have receivers
    std::vector<std::string> recvs = getReceivers();
    for(std::vector<std::string>::const_iterator it = recvs.begin(); it != recvs.end(); it++)
    {
        registerService(*it);
    }
    
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
    
    // Explicitly deregister all services
    std::vector<std::string> recvs = getReceivers();
    for(std::vector<std::string>::const_iterator it = recvs.begin(); it != recvs.end(); it++)
    {
        deregisterService(*it);
    }
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////

void MessageTransportTask::cleanupHook()
{
    MessageTransportTaskBase::cleanupHook();

    delete mUDTNode;
    mUDTNode = NULL;

    delete mDistributedServiceDirectory;
    mDistributedServiceDirectory = NULL;

    delete mMessageTransport;
    mMessageTransport = NULL;
    
    fipa::services::tcp::SocketTransport::stopListening();
}

fipa::acl::AgentIDList MessageTransportTask::deliverLetterLocally(const fipa::acl::Letter& letter)
{
    using namespace fipa::acl;

    ACLBaseEnvelope envelope = letter.flattened();
    AgentIDList receivers = envelope.getIntendedReceivers();
    AgentIDList::const_iterator rit = receivers.begin();

    // Maintain a list of remaining receivers
    AgentIDList remainingReceivers = receivers;

    // For each intended receiver, try to deliver.
    for(; rit != receivers.end(); ++rit)
    {
        // Handle delivery
        // The name of the next destination, can be an intermediate receiver
        std::string receiverName = rit->getName();
        fipa::acl::Letter updatedLetter = letter.createDedicatedEnvelope( fipa::acl::AgentID(receiverName) );

        // Check for local receivers 
        // XXX duplicate: done in each forwarding method
        fipa::services::ServiceDirectoryList list = mDistributedServiceDirectory->search(receiverName, fipa::services::ServiceDirectoryEntry::NAME, false);
        if(list.empty())
        {
            RTT::log(RTT::Warning) << "MessageTransportTask: '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' since it is globally and locally unknown" << RTT::endlog();
            continue;
        } else if(list.size() > 1) {
            RTT::log(RTT::Warning) << "MessageTransportTask: '" << getName() << "' : receiver '" << receiverName << "' has multiple entries in the service directory -- cannot disambiguate'" << RTT::endlog();
        } else {
            using namespace fipa::services;
            // Extract the service location
            ServiceDirectoryEntry serviceEntry = list.front();
            ServiceLocator locator = serviceEntry.getLocator();
            ServiceLocation location = locator.getFirstLocation();
            
            // Check against the first service location (should be enough)
            if(location == mDefaultTransport->getServiceLocations()[0])
            {
                // Local delivery
                RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' delivery to local client" << RTT::endlog();

                // Deliver the message to local client, i.e. the corresponding receiver has a dedicated output port available on this MTS
                ReceiverPorts::iterator portsIt = mReceivers.find(receiverName);
                if(portsIt == mReceivers.end())
                {
                    RTT::log(RTT::Warning) << "MessageTransportTask: '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' due to an internal error. No port is available for this receiver." << RTT::endlog();
                    continue;
                } else {
                    RTT::OutputPort<fipa::SerializedLetter>* clientPort = dynamic_cast< RTT::OutputPort<fipa::SerializedLetter>* >(portsIt->second);
                    if(clientPort)
                    {
                        fipa::SerializedLetter serializedLetter(updatedLetter, fipa::acl::representation::BITEFFICIENT);
                        if(!clientPort->connected())
                        {
                            RTT::log(RTT::Error) << "MessageTransportTask: '" << getName() << "' : client port to '" << receiverName << "' exists, but is not connected" << RTT::endlog();
                            continue;
                        } else {
                            clientPort->write(serializedLetter);
                            // Remove the receiver
                            fipa::acl::AgentIDList::iterator it = std::find(remainingReceivers.begin(), remainingReceivers.end(), *rit); 
                            if(it != remainingReceivers.end())
                            {
                                remainingReceivers.erase(it);
                            }

                            RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' : delivery to '" << receiverName << "'" << RTT::endlog();
                            continue;
                        }
                    } else {
                        RTT::log(RTT::Error) << "MessageTransportTask: '" << getName() << "' : internal error since client port could not be casted to expected type" << RTT::endlog();
                        continue;
                    }
                }
            }
            else
            {
                RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' : not handling '" << receiverName << "' as it's not a local agent." << RTT::endlog();
            }
        }
    }

    return remainingReceivers;
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
    RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : adding receiver '" << receiver << "'" << RTT::endlog();
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
        registerService(receiver);
    }

    return success;
}

void MessageTransportTask::deregisterService(std::string receiver)
{
    //FIXME
    try
    {
        RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : deregistering service '" << receiver << "'" << RTT::endlog();
        mDistributedServiceDirectory->deregisterService(receiver, fipa::services::ServiceDirectoryEntry::NAME);
    }
    catch(const fipa::services::NotFound& e)
    {
        RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : deregistering service '" << receiver << "' failed: not found" << RTT::endlog();
    }
}

void MessageTransportTask::registerService(std::string receiver)
{
    //FIXME
    RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : registering service '" << receiver << "'" << RTT::endlog();
    fipa::services::ServiceLocator locator;
        
    std::vector<fipa::services::ServiceLocation> locations = mDefaultTransport->getServiceLocations();
    for(std::vector<fipa::services::ServiceLocation>::const_iterator it = locations.begin();
        it != locations.end(); it++)
    {
        locator.addLocation(*it);
    }

    fipa::services::ServiceDirectoryEntry client(receiver, "mts_client", locator, "Message client of " + getName());
    mDistributedServiceDirectory->registerService(client);
}


bool MessageTransportTask::removeReceiver(::std::string const & receiver)
{
    using namespace fipa::services;
    if(removeReceiverPort(receiver))
    {
        deregisterService(receiver);
        return true;
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

} // namespace fipa_services
