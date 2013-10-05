#include "MessageTransportTask.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <boost/bind.hpp>
#include <boost/thread/locks.hpp>

#include <fipa_acl/fipa_acl.h>
#include <fipa_services/MessageTransport.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <numeric/Stats.hpp>

namespace rc = RTT::corba;

using namespace RTT;

namespace fipa_services
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
MessageTransportTask::MessageTransportTask(std::string const& name) 
    : MessageTransportTaskBase(name)
{
}

MessageTransportTask::MessageTransportTask(std::string const& name, RTT::ExecutionEngine* engine)
    : MessageTransportTaskBase(name, engine)
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

    // Create internal message transport service
    fipa::acl::AgentID agentName(this->getName());
    mMessageTransport = new fipa::service::message_transport::MessageTransport(agentName);
    // register the default transport
    mMessageTransport->registerTransport("default-corba-transport", boost::bind(&MessageTransportTask::deliverOrForwardLetter,this,_1));

    return true;
}

void MessageTransportTask::errorHook()
{
    MessageTransportTaskBase::errorHook();
    RTT::log(RTT::Error) << "MessageTransportService: Entering error state." << RTT::endlog();
}

bool MessageTransportTask::startHook()
{
    RTT::log(RTT::Info) << "MessageTransportService: startHook" << RTT::endlog();
    if(!MessageTransportTaskBase::startHook())
        return false;

    return true;
}

void MessageTransportTask::updateHook()
{
    using namespace fipa::acl;

    RTT::log(RTT::Warning) << "MessageTransportService: Update hook called of '" << getName() << "'" << RTT::endlog();

    fipa::SerializedLetter serializedLetter;
    while( _letters.read(serializedLetter) == RTT::NewData)
    {
        RTT::log(RTT::Debug) << "MessageTransportService '" << getName() << "' : received new letter of size '" << serializedLetter.getVector().size() << "'" << RTT::endlog();

        fipa::acl::Letter letter = serializedLetter.deserialize();
        fipa::acl::ACLBaseEnvelope be = letter.flattened();
        RTT::log(RTT::Debug) << "MessageTransportService '" << getName() << "' : intended receivers: " << be.getIntendedReceivers() << ", content: " << letter.getACLMessage().getContent() << RTT::endlog();
        mMessageTransport->handle(letter);
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
        RTT::log(RTT::Debug) << "MessageTransportTask: deliverOrForwardLetter to: " << rit->getName() << RTT::endlog();

        // Handle delivery
        std::string receiverName = rit->getName();
        ReceiverPorts::iterator portsIt = mReceivers.find(receiverName);

        if(portsIt == mReceivers.end())
        {
            try {
                // this is probably a local agent without direct connection, thus send to the mts
                std::string mtsName = mMessageTransport->getResponsibleMessageTransport(receiverName);
                portsIt = mReceivers.find(mtsName);
            } catch(const std::runtime_error& e)
            {
                RTT::log(RTT::Warning) << "MessageTransportService '" << getName() << "' : could neither deliver nor forward message to receiver, since it is not a local receiver and route is unknown' " << receiverName << "'" << RTT::endlog();
                return false;
            }
        }

        if(portsIt != mReceivers.end())
        {
            RTT::OutputPort<fipa::SerializedLetter>* clientPort = dynamic_cast< RTT::OutputPort<fipa::SerializedLetter>* >(portsIt->second);
            if(clientPort)
            {
                if(!clientPort->connected())
                {
                    RTT::log(RTT::Error) << "MessageTransportService: port to '" << rit->getName() << "' is not connected" << RTT::endlog();
                    continue;
                }

                fipa::acl::Letter updatedLetter = letter;
                fipa::acl::ACLBaseEnvelope extraEnvelope;
                AgentIDList intendedReceivers;
                intendedReceivers.push_back(fipa::acl::AgentID(receiverName));
                extraEnvelope.setIntendedReceivers(intendedReceivers);
                updatedLetter.addExtraEnvelope(extraEnvelope);

                fipa::SerializedLetter serializedLetter(updatedLetter, fipa::acl::representation::BITEFFICIENT);
                clientPort->write(serializedLetter);
                RTT::log(RTT::Debug) << "MessageTransportService: delivery to '" << receiverName << "' performed by '" << getName() << "'" << RTT::endlog();
                continue;
            } else {
                RTT::log(RTT::Error) << "MessageTransportService: client port with unexpected type" << RTT::endlog();
                return false;
            }
        } else {
            RTT::log(RTT::Error) << "MessageTransportTask::deliverOrForwardLetter: could neither deliver nor forward message to receiver, since it is not a local receiver and route is unknown: '" << receiverName << RTT::endlog();
        }
    }

    return trigger();
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
        log(Info) << "connection " << receiver << " is already registered" << endlog();
        // Since the connection already exists, everything is good?! 
        // so could be true here as well
        return false;
    }   

    RTT::base::PortInterface* port = _letters.antiClone();
    port->setName(receiver);
    
    RTT::base::OutputPortInterface *out_port = dynamic_cast<RTT::base::OutputPortInterface*>(port);
    if(!out_port)
    {
        log(Error) << "could not cast anticlone to outputport" << endlog();
        return false;
    }
        
    bool success = addReceiverPort(out_port, receiver);
    if(success)
    {
        if(is_local) 
        {
            mLocalReceivers.push_back(receiver);
            publishConnectionStatus();
        }
    }

    return success;
}


bool MessageTransportTask::removeReceiver(::std::string const & receiver)
{
        if(removeReceiverPort(receiver))
        {
            std::vector<std::string>::iterator it = std::find(mLocalReceivers.begin(), mLocalReceivers.end(), receiver);
            if(it != mLocalReceivers.end())
            {
                mLocalReceivers.erase(it);
                publishConnectionStatus();
            }
            return true;
        }

        return false;
}

bool MessageTransportTask::addReceiverPort(RTT::base::OutputPortInterface* outputPort, const std::string& receiver)
{
    boost::unique_lock<boost::shared_mutex> lock(mServiceChangeMutex);

    ports()->addPort(outputPort->getName(), *outputPort);
    mReceivers[receiver] = outputPort;
    RTT::log(RTT::Debug) << "MessageTransportService: Receiver port '" << receiver << "' added" << RTT::endlog();
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

    log(Info) << "No output port named '" << receiver << "' registered" << endlog();

    return false;
}

void MessageTransportTask::publishConnectionStatus()
{
    // send a FIPA message with INFORM - and internal-mta conversation id update on the mts services known to this mts
    std::vector<std::string> receivers;
    std::vector<std::string> localAgents;
    ReceiverPorts::const_iterator cit = mReceivers.begin();
    for(; cit != mReceivers.end(); ++cit)
    {
        receivers.push_back(cit->first);
    }

    if(mMessageTransport)
    {
        mMessageTransport->publishConnectionStatus(receivers, mLocalReceivers);
    } else {
        RTT::log(RTT::Error) << "MessageTransportService: not initialized for '" << getName() << "', thus not publishing status" << RTT::endlog();
    }
}

} // namespace fipa_services

