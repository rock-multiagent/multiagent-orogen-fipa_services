#include "MessageTransportTask.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/thread/locks.hpp>

#include <fipa_services/MessageTransport.hpp>
#include <fipa_services/ServiceDirectory.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>

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
    , mGlobalServiceDirectory(0)
    , mLocalServiceDirectory(0)
    , mLocalServiceDirectoryTimestamp( ::base::Time::now() )
{
}

MessageTransportTask::MessageTransportTask(std::string const& name, RTT::ExecutionEngine* engine)
    : MessageTransportTaskBase(name, engine)
    , mMessageTransport(0)
    , mGlobalServiceDirectory(0)
    , mLocalServiceDirectory(0)
    , mLocalServiceDirectoryTimestamp( ::base::Time::now() )
{
}

MessageTransportTask::~MessageTransportTask()
{
}

////////////////////////////////HOOKS///////////////////////////////
bool MessageTransportTask::configureHook()
{
    if(!MessageTransportTaskBase::configureHook())
    {
        return false;
    }

    //Service service(mts::FIPA_CORBA_TRANSPORT_SERVICE, this);

    mGlobalServiceDirectory = new fipa::services::ServiceDirectory();
    mLocalServiceDirectory = new fipa::services::ServiceDirectory();

    // Create internal message transport service
    fipa::acl::AgentID agentName(this->getName());
    mMessageTransport = new fipa::services::message_transport::MessageTransport(agentName);
    // register the default transport
    mMessageTransport->registerTransport("default-corba-transport", boost::bind(&MessageTransportTask::deliverOrForwardLetter,this,_1));

    return true;
}

void MessageTransportTask::errorHook()
{
    MessageTransportTaskBase::errorHook();
    RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "'" << " : Entering error state." << RTT::endlog();
}

bool MessageTransportTask::startHook()
{
    RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : startHook" << RTT::endlog();
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

    // Check updates of other mts, each update should be valid for only one locator (the sending mts)
    fipa::services::ServiceDirectoryList directoryUpdate;
    while(_service_directories.read(directoryUpdate) == RTT::NewData)
    {
        mGlobalServiceDirectory->mergeSelectively(directoryUpdate, fipa::services::ServiceDirectoryEntry::LOCATOR);
        _global_service_directory.write(mGlobalServiceDirectory->getAll());
    }

    // Check if new receives have been registered or have been removed between
    // now and the last update
    if(mLocalServiceDirectoryTimestamp < mLocalServiceDirectory->getTimestamp())
    {
        _local_service_directory.write(mLocalServiceDirectory->getAll());
        mLocalServiceDirectoryTimestamp = ::base::Time::now();
    }
}

void MessageTransportTask::stopHook()
{
    MessageTransportTaskBase::stopHook();
}

bool MessageTransportTask::deliverOrForwardLetter(const fipa::acl::Letter& letter)
{
    using namespace fipa::acl;

    ACLBaseEnvelope envelope = letter.flattened();
    AgentIDList receivers = envelope.getIntendedReceivers();
    AgentIDList::const_iterator rit = receivers.begin();
    for(; rit != receivers.end(); ++rit)
    {
        RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : deliverOrForwardLetter to: " << rit->getName() << RTT::endlog();

        // Handle delivery
        std::string receiverName = rit->getName();
        std::string intendedReceiverName = receiverName;

        fipa::services::ServiceDirectoryList list = mLocalServiceDirectory->search(receiverName, fipa::services::ServiceDirectoryEntry::NAME, false);
        if(list.empty())
        {
            fipa::services::ServiceDirectoryList list = mGlobalServiceDirectory->search(receiverName, fipa::services::ServiceDirectoryEntry::NAME, false);
            if(!list.empty())
            {
                fipa::services::Locator locator = list[0].getLocator();
                RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : receiver '" << receiverName << "' is globally known via: '" << locator << "'"  << RTT::endlog();
                receiverName = locator;
            } else {
                RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' since it is globally and locally unknown" << RTT::endlog();
                return false;
            }
        } else {
            RTT::log(RTT::Info) << "MessageTransportTask '" << getName() << "' : receiver '" << receiverName << "' is locally attached'" << RTT::endlog();
        }

        ReceiverPorts::iterator portsIt = mReceivers.find(receiverName);
        if(portsIt == mReceivers.end())
        {
            RTT::log(RTT::Warning) << "MessageTransportTask '" << getName() << "' : could neither deliver nor forward message to receiver: '" << receiverName << "' due to an internal error. No port is available for this receiver." << RTT::endlog();
            return false;
        } else {
            RTT::OutputPort<fipa::SerializedLetter>* clientPort = dynamic_cast< RTT::OutputPort<fipa::SerializedLetter>* >(portsIt->second);
            if(clientPort)
            {
                if(!clientPort->connected())
                {
                    RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : port to '" << receiverName << "' is not connected" << RTT::endlog();
                    continue;
                }

                fipa::acl::Letter updatedLetter = letter.createDedicatedEnvelope( fipa::acl::AgentID(intendedReceiverName) );
                fipa::SerializedLetter serializedLetter(updatedLetter, fipa::acl::representation::BITEFFICIENT);
                clientPort->write(serializedLetter);
                RTT::log(RTT::Debug) << "MessageTransportTask '" << getName() << "' : delivery to '" << receiverName << "' (indendedReceiver is '" << intendedReceiverName << "')" << RTT::endlog();
                continue;
            } else {
                RTT::log(RTT::Error) << "MessageTransportTask '" << getName() << "' : client port with unexpected type" << RTT::endlog();
                return false;
            }
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////

void MessageTransportTask::cleanupHook()
{
    MessageTransportTaskBase::cleanupHook();

    delete mLocalServiceDirectory;
    mLocalServiceDirectory = NULL;

    delete mGlobalServiceDirectory;
    mGlobalServiceDirectory = NULL;

    delete mMessageTransport;
    mMessageTransport = NULL;

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
        fipa::services::ServiceDirectoryEntry client(receiver, "mts_client", getName() , "Message client of " + getName());
        mLocalServiceDirectory->registerService(client);
    }

    // Update service directory
    trigger();
    return success;
}


bool MessageTransportTask::removeReceiver(::std::string const & receiver)
{
    using namespace fipa::services;
    if(removeReceiverPort(receiver))
    {
        ServiceDirectoryList list = mLocalServiceDirectory->search(getName(), ServiceDirectoryEntry::LOCATOR);
        for(size_t i = 0; i < list.size(); ++i)
        {
            if( list[i].getName() == receiver)
            {
                mLocalServiceDirectory->deregisterService(receiver, ServiceDirectoryEntry::NAME);
            }
        }
    }

    // Update service directory
    trigger();
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

} // namespace fipa_services

