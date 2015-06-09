#include "MessageTransportTask.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>

#include <fipa_services/MessageTransport.hpp>
#include <fipa_services/DistributedServiceDirectory.hpp>
#include <fipa_services/transports/Transport.hpp>

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
{
    initializeMessageTransport();
}

MessageTransportTask::MessageTransportTask(std::string const& name, RTT::ExecutionEngine* engine)
    : MessageTransportTaskBase(name, engine)
    , mMessageTransport(0)
{
    initializeMessageTransport();
}

MessageTransportTask::~MessageTransportTask()
{}

void MessageTransportTask::initializeMessageTransport()
{
    // Make sure the MTS name is unique for each running instance
    uuid_t uuid;
    uuid_generate(uuid);
    char mtsUID[512];
    uuid_unparse(uuid, mtsUID);

    fipa::acl::AgentID agentName(this->getName() + "-" + std::string(mtsUID));
    fipa::services::ServiceDirectory::Ptr serviceDirectory(new fipa::services::DistributedServiceDirectory());
    mMessageTransport = new fipa::services::message_transport::MessageTransport(agentName, serviceDirectory);
}

////////////////////////////////HOOKS///////////////////////////////
bool MessageTransportTask::configureHook()
{
    if(!MessageTransportTaskBase::configureHook())
    {
        return false;
    }

    // After cleanup
    if(mMessageTransport == 0)
    {
        initializeMessageTransport();
    }

    // Apply transport configurations
    std::vector<fipa::services::transports::Configuration> transport_configurations = _transport_configurations.get();
    mMessageTransport->configure(transport_configurations);

    // Transport activation based on the selected set of transports
    std::vector<std::string> transports = _transports.get();
    if(transports.empty())
    {
        transports.push_back("UDT");
    }
    mMessageTransport->activateTransports(transports);

    // Register local message transport
    mMessageTransport->registerMessageTransport("local-delivery",
                                         boost::bind(&fipa_services::MessageTransportTask::deliverLetterLocally, this,_1,_2));

    // // Fill the list of other service locations from the configuration property.
    // std::vector<std::string> addresses = _known_addresses.get();
    // for(std::vector<std::string>::const_iterator it = addresses.begin(); it != addresses.end(); ++it)
    // {
    //     // Split by the '='
    //     std::vector<std::string> tokens;
    //     boost::split(tokens, *it, boost::is_any_of("="));

    //     fipa::services::ServiceLocator locator;
    //     fipa::services::ServiceLocation location = fipa::services::ServiceLocation(tokens[1], "fipa::services::udt::UDTTransport");
    //     locator.addLocation(location);

    //     fipa::services::ServiceDirectoryEntry entry (tokens[0], mClientServiceType, locator, "Message client");
    //     //mDistributedServiceDirectory->registerService(entry);
    //     otherServiceDirectoryEntries.push_back(entry);
    // }

    // Create the output ports for known local receivers
    // This will create the necessary set of output ports
    std::vector<std::string> localReceivers = _local_receivers.get();
    for(std::vector<std::string>::const_iterator it = localReceivers.begin(); it != localReceivers.end(); ++it)
    {
        if( !addReceiver(*it, true) )
        {
            RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "'" << ": adding output port for local receiver '" << *it << "' failed";
            return false;
        }
    }

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

    // And register other already known addresses (e.g. when they cannot be
    // discovery using the distributed name service)
    for(std::vector<fipa::services::ServiceDirectoryEntry>::const_iterator it = mExtraServiceDirectoryEntries.begin(); it != mExtraServiceDirectoryEntries.end(); it++)
    {
        mMessageTransport->getServiceDirectory()->registerService(*it);
    }

    return true;
}

void MessageTransportTask::updateHook()
{
    using namespace fipa::acl;

    // Handling the incoming letters from direct clients
    fipa::SerializedLetter serializedLetter;
    while( _letters.read(serializedLetter) == RTT::NewData)
    {
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : received new letter of size '" << serializedLetter.getVector().size() << "'" << RTT::endlog();

        fipa::acl::Letter letter = serializedLetter.deserialize();

        // Debugging
        fipa::acl::ACLBaseEnvelope be = letter.flattened();
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : intended receivers: " << be.getIntendedReceivers() << ", content: " << letter.getACLMessage().getContent() << RTT::endlog();

        // Handle letter
        mMessageTransport->handle(letter);
    }

    // trigger connection handling and message processing
    mMessageTransport->trigger();
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

    // And deregister other known addresses
    for(std::vector<fipa::services::ServiceDirectoryEntry>::const_iterator it = mExtraServiceDirectoryEntries.begin(); it != mExtraServiceDirectoryEntries.end(); it++)
    {
        deregisterService(it->getName());
    }
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////

void MessageTransportTask::cleanupHook()
{
    MessageTransportTaskBase::cleanupHook();

    delete mMessageTransport;
    mMessageTransport = NULL;
}

bool MessageTransportTask::deliverLetterLocally(const std::string& receiverName, const fipa::acl::Letter& letter)
{
    // Local delivery
    RTT::log(RTT::Debug) << "MessageTransportTask: '" << getName() << "' delivery to local client" << RTT::endlog();

    // Deliver the message to local clients, i.e. a corresponding receiver has a dedicated output port available on this MTS
    ReceiverPorts::iterator portsIt = mReceivers.find(receiverName);
    if(portsIt == mReceivers.end())
    {
        RTT::log(RTT::Warning) << "MessageTransportTask: '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' due to an internal error. No port is available for this receiver." << RTT::endlog();
    } else {
        RTT::OutputPort<fipa::SerializedLetter>* clientPort = dynamic_cast< RTT::OutputPort<fipa::SerializedLetter>* >(portsIt->second);
        if(clientPort)
        {
            fipa::SerializedLetter serializedLetter(letter, fipa::acl::representation::BITEFFICIENT);
            if(!clientPort->connected())
            {
                RTT::log(RTT::Error) << "MessageTransportTask: '" << getName() << "' : client port to '" << receiverName << "' exists, but is not connected" << RTT::endlog();
            } else {
                clientPort->write(serializedLetter);
                return true;
            }
        } else {
            RTT::log(RTT::Error) << "MessageTransportTask: '" << getName() << "' : internal error since client port could not be casted to expected type" << RTT::endlog();
        }
    }

    return false;
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
    try
    {
        RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : deregistering service '" << receiver << "'" << RTT::endlog();
        mMessageTransport->deregisterClient(receiver);
    }
    catch(const fipa::services::NotFound& e)
    {
        RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : deregistering service '" << receiver << "' failed: not found" << RTT::endlog();
    }
}

void MessageTransportTask::registerService(std::string receiver)
{
    RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : registering service '" << receiver << "'" << RTT::endlog();
    mMessageTransport->registerClient(receiver, "Message client of " + getName());
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
