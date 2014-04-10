#include "MessageTransportTask.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <fipa_services/MessageTransport.hpp>
#include <fipa_services/SocketTransport.hpp>
#include <fipa_services/DistributedServiceDirectory.hpp>
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
    , mServiceLocation(0)
    , mUDTNode(0)
{}

MessageTransportTask::MessageTransportTask(std::string const& name, RTT::ExecutionEngine* engine)
    : MessageTransportTaskBase(name, engine)
    , mMessageTransport(0)
    , mDistributedServiceDirectory(0)
    , mServiceLocation(0)
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
    // register the default transport
    mMessageTransport->registerTransport("default-corba-transport", boost::bind(&MessageTransportTask::deliverOrForwardLetter,this,_1));

    mUDTNode = new fipa::services::udt::Node();
    mUDTNode->listen();
    mInterface = _nic.get();

    // This MTA's service location and thus for all of its clients
    //
    // When required the service address will be used to connect to this service, which has to
    // handle all connection requests and incoming messages
    mServiceLocation = new fipa::services::ServiceLocation(mUDTNode->getAddress(mInterface).toString(), this->getModelName());

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

fipa::acl::AgentIDList MessageTransportTask::deliverOrForwardLetter(const fipa::acl::Letter& letter)
{
    using namespace fipa::acl;


    ACLBaseEnvelope envelope = letter.flattened();
    AgentIDList receivers = envelope.getIntendedReceivers();
    AgentIDList::const_iterator rit = receivers.begin();

    AgentIDList remainingReceivers = receivers;

    // For each intended receiver, try to deliver.
    // If it is a local client, deliver locally, otherwise forward to the known locator, which is an MTS in this context
    for(; rit != receivers.end(); ++rit)
    {
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : deliverOrForwardLetter to: " << rit->getName() << RTT::endlog();

        // Handle delivery
        // The name of the next destination, can be an intermediate receiver
        std::string receiverName = rit->getName();
        // The intended receiver, i.e. name of the final destination
        std::string intendedReceiverName = receiverName;
        fipa::acl::Letter updatedLetter = letter.createDedicatedEnvelope( fipa::acl::AgentID(intendedReceiverName) );

        // Check for local receivers, or identify locator
        fipa::services::ServiceDirectoryList list = mDistributedServiceDirectory->search(receiverName, fipa::services::ServiceDirectoryEntry::NAME, false);
        if(list.empty())
        {
            RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' since it is globally and locally unknown" << RTT::endlog();

            // Cleanup existing entries
            std::map<std::string, fipa::services::udt::OutgoingConnection*>::const_iterator cit = mMTSConnections.find(receiverName);
            if(cit != mMTSConnections.end())
            {
                delete cit->second;
                mMTSConnections.erase(receiverName);
            }
            continue;
        } else if(list.size() > 1) {
            RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : receiver '" << receiverName << "' has multiple entries in the service directory -- cannot disambiguate'" << RTT::endlog();
        } else {
            using namespace fipa::services;
            // Extract the service location
            ServiceDirectoryEntry serviceEntry = list.front();
            ServiceLocator locator = serviceEntry.getLocator();
            ServiceLocation location = locator.getFirstLocation();

            if( location.getSignatureType() != this->getModelName())
            {
                RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : service signature for '" << receiverName << "' is '" << location.getSignatureType() << "' but expected '" << this->getModelName() << "' -- will not connect: " << serviceEntry.toString() << RTT::endlog();
                continue;
            }

            // Local delivery
            if(location == *mServiceLocation)
            {
                RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' delivery to local client" << RTT::endlog();

                // Deliver the message to local client, i.e. the corresponding receiver has a dedicated output port available on this MTS
                ReceiverPorts::iterator portsIt = mReceivers.find(receiverName);
                if(portsIt == mReceivers.end())
                {
                    RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' due to an internal error. No port is available for this receiver." << RTT::endlog();
                    continue;
                } else {
                    RTT::OutputPort<fipa::SerializedLetter>* clientPort = dynamic_cast< RTT::OutputPort<fipa::SerializedLetter>* >(portsIt->second);
                    if(clientPort)
                    {
                        fipa::SerializedLetter serializedLetter(updatedLetter, fipa::acl::representation::BITEFFICIENT);
                        if(!clientPort->connected())
                        {
                            RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : client port to '" << receiverName << "' exists, but is not connected" << RTT::endlog();
                            continue;
                        } else {
                            clientPort->write(serializedLetter);

                            fipa::acl::AgentIDList::iterator it = std::find(remainingReceivers.begin(), remainingReceivers.end(), receiverName);
                            if(it != remainingReceivers.end())
                            {
                                remainingReceivers.erase(it);
                            }

                            RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : delivery to '" << receiverName << "' (indendedReceiver is '" << intendedReceiverName << "')" << RTT::endlog();
                            continue;
                        }
                    } else {
                        RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : internal error since client port could not be casted to expected type" << RTT::endlog();
                        continue;
                    }
                }
            } else {
                RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' forwarding to other MTS" << RTT::endlog();

                // Sending message to another MTS
                std::map<std::string, fipa::services::udt::OutgoingConnection*>::const_iterator cit = mMTSConnections.find(receiverName);
                udt::Address address = udt::Address::fromString(location.getServiceAddress());

                bool connectionExists = false;
                // Validate connection by comparing address in cache and current address in service directory
                udt::OutgoingConnection* mtsConnection = 0;
                if(cit != mMTSConnections.end())
                {
                    mtsConnection = cit->second;
                    if(mtsConnection->getAddress() != address)
                    {
                        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : cached connection requires an update " << mtsConnection->getAddress().toString() << " vs. " << address.toString() << " -- deleting existing entry" << RTT::endlog();

                        delete cit->second;
                        mMTSConnections.erase(receiverName);
                    } else {
                        connectionExists = true;
                    }
                }

                // Cache newly created connection
                if(!connectionExists)
                {
                    try {
                        mtsConnection = new udt::OutgoingConnection(address);
                        mMTSConnections[receiverName] = mtsConnection;
                    } catch(const std::runtime_error& e)
                    {
                        RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : could not establish connection to '" << location.toString() << "' -- " << e.what() << RTT::endlog();
                        continue;
                    }
                }

                RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' : sending letter to '" << receiverName << "'" << RTT::endlog();
                try {
                    mtsConnection->sendLetter(updatedLetter);

                    fipa::acl::AgentIDList::iterator it = std::find(remainingReceivers.begin(), remainingReceivers.end(), receiverName);
                    if(it != remainingReceivers.end())
                    {
                        remainingReceivers.erase(it);
                    }
                } catch(const std::runtime_error& e)
                {
                    RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : could not send letter to '" << receiverName << "' -- " << e.what() << RTT::endlog();
                    continue;
                }
            } // end handling
        }
    }

    return remainingReceivers;
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////

void MessageTransportTask::cleanupHook()
{
    MessageTransportTaskBase::cleanupHook();

    delete mUDTNode;
    mUDTNode = NULL;

    delete mServiceLocation;
    mServiceLocation = NULL;

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
        locator.addLocation(*mServiceLocation);

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
    // create socket transport TODO params
    mSocketTransport = new fipa::services::message_transport::SocketTransport(mMessageTransport);
    // register socket transport
    mMessageTransport->registerTransport("socket-transport", 
                                         boost::bind(&fipa::services::message_transport::SocketTransport::deliverForwardLetter, mSocketTransport, _1));
}

} // namespace fipa_services
