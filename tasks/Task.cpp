#include "Task.hpp"

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

namespace sd = servicediscovery;
namespace rc = RTT::corba;

using namespace RTT;

namespace mts
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
Task::Task(std::string const& name) : TaskBase(name),
        mServiceDiscovery(NULL),
        mConnectionsMutex()
{

    Service service(mts::FIPA_CORBA_TRANSPORT_SERVICE, this);
}

Task::Task(std::string const& name, RTT::ExecutionEngine* engine) : TaskBase(name, engine), 
        mServiceDiscovery(NULL),
        mConnectionsMutex()
{

    Service service(mts::FIPA_CORBA_TRANSPORT_SERVICE, this);
}

Task::~Task()
{
}

////////////////////////////////HOOKS///////////////////////////////
bool Task::configureHook()
{
    if(!TaskBase::configureHook())
    {
        return false;
    }

    // Create internal message transport service
    fipa::acl::AgentID agentName(this->getName());
    mMessageTransport = new fipa::service::message_transport::MessageTransport(agentName);
    // register the default transport
    mMessageTransport->registerTransport("default-corba-transport", boost::bind(&Task::deliverOrForwardLetter,this,_1));

    // If configure properties are empty, module will be stopped.
    if(_avahi_type.get().empty() || _avahi_port.get() == 0)
    {
        RTT::log(RTT::Info) << "MessageTransportService: Properties are not set, module will be stopped." << RTT::endlog();
        return false;
    }

    mConnectionBufferSize = _connection_buffer_size.get();
    return true;
}

void Task::errorHook()
{
    TaskBase::errorHook();
    RTT::log(RTT::Error) << "MessageTransportService: Entering error state." << RTT::endlog();
}

bool Task::startHook()
{
    if(!TaskBase::startHook())
        return false;


    sd::ServiceConfiguration sc(this->getName(), _avahi_type.get(), _avahi_port.get());
    sc.setTTL(_avahi_ttl.get());
    sc.setDescription("IOR", rc::TaskContextServer::getIOR(this));
    mServiceDiscovery = new sd::ServiceDiscovery();

    // Add callback functions.
    mServiceDiscovery->addedComponentConnect(sigc::mem_fun(*this, 
        &Task::serviceAdded));
    mServiceDiscovery->removedComponentConnect(sigc::mem_fun(*this, 
        &Task::serviceRemoved));

    // Start SD.
    try{
        mServiceDiscovery->start(sc);
    } catch(const std::exception& e) {
        RTT::log(RTT::Error) << e.what() << RTT::endlog();
    }

   RTT::log(RTT::Info) << "MessageTransportService: Started service '" << this->getName() << ". Avahi-type: '" << _avahi_type.get() << "'. Port: " << _avahi_port.get() << ". TTL: " << _avahi_ttl.get() << "." << RTT::endlog();



    return true;
}

void Task::updateHook()
{
    using namespace fipa::acl;

    const RTT::DataFlowInterface::Ports& ports = this->ports()->getPorts();

    for(RTT::DataFlowInterface::Ports::const_iterator it = ports.begin(); it != ports.end(); it++)
    { 
		RTT::InputPort<fipa::SerializedLetter>* letter_port = dynamic_cast< RTT::InputPort<fipa::SerializedLetter>* >(*it);
		fipa::SerializedLetter serializedLetter;
		if(letter_port)
		{
            std::string portName = letter_port->getName();
            uint32_t count = 0;
			while( letter_port->read(serializedLetter) == RTT::NewData)
			{
                RTT::log(RTT::Debug) << "MessageTransportService: Received new message on port " << portName << "' of size '" << serializedLetter.getVector().size() << "'" << RTT::endlog();

                fipa::acl::Letter letter = serializedLetter.deserialize();
                fipa::acl::ACLBaseEnvelope be = letter.flattened();
                RTT::log(RTT::Debug) << "Receivers: " << be.getTo() << "content: " << letter.getACLMessage().getContent() << RTT::endlog();
                mMessageTransport->handle(letter);
                ++count;
			}
            mPortStats[portName].update(count);
		}
    }

    // Handle open connection request
    std::vector<std::string> remote_ids;
    {
        boost::unique_lock<boost::shared_mutex> lock(mConnectionsMutex);
        std::map<std::string, ConnectionInterface*>::iterator it = mConnections.begin();
        for(; it != mConnections.end(); ++it)
        {
            ConnectionInterface* connection = it->second;
            if(!connection->isConnected())
            {
                try {
                    addReceiver(connection->getReceiverName());
                    if( connection->connect() )
                    {
                        // Add port for receiver
                        RTT::log(RTT::Info) << "MessageTransportService: Connected to '" << connection->getReceiverName() << "'" << RTT::endlog();
                    }
                    remote_ids.push_back(connection->getReceiverName());
                } catch(...)
                {
                    RTT::log(RTT::Debug) << "Connection failed" << RTT::endlog();
                }
            }
        }
    }

    //std::vector<std::string>::const_iterator cit = remote_ids.begin();
    //for(; cit != remote_ids.end(); ++cit)
    //{
    //    //triggerRemoteAgentListUpdate(*cit);
    //}

    std::map<std::string, ::base::Stats<double> >::iterator statsIt = mPortStats.begin();
    std::vector<mts::PortStats> portStats;
    for(; statsIt != mPortStats.end(); ++statsIt)
    {
        portStats.push_back(mts::PortStats(statsIt->first, statsIt->second));
    }

    _port_stats.write(portStats);
}

bool Task::isConnectedTo(const std::string& name) const
{
    boost::shared_lock<boost::shared_mutex> lock(mConnectionsMutex);

    std::map<std::string, ConnectionInterface*>::const_iterator it;
    it = mConnections.find(name);
    return it != mConnections.end();
}

bool Task::deliverOrForwardLetter(const fipa::acl::Letter& letter)
{
    using namespace fipa::acl;
    // Identifing the connection
    ACLBaseEnvelope envelope = letter.flattened();
    // TODO: use intended receivers and update the field
    //AgentIDList receivers = envelope.getIntendedReceivers();
    AgentIDList receivers = envelope.getTo();

    AgentIDList::const_iterator rit = receivers.begin();


    for(; rit != receivers.end(); ++rit)
    {
        // Handle local delivery
        ReceiverPorts::iterator localPortsIt =  mReceivers.find(rit->getName());
        if(localPortsIt != mReceivers.end())
        {
            RTT::OutputPort<fipa::SerializedLetter>* clientPort = dynamic_cast< RTT::OutputPort<fipa::SerializedLetter>* >(localPortsIt->second);
            if(clientPort)
            {
                fipa::SerializedLetter serializedLetter(letter, fipa::acl::representation::BITEFFICIENT);
                clientPort->write(serializedLetter);
                RTT::log(RTT::Debug) << "MessageTransportService: local delivery to '"<< rit->getName() << "' performed" << RTT::endlog();
                continue;
            } else {
                RTT::log(RTT::Error) << "MessageTransportService: Client port with unexpected type" << RTT::endlog();
                return false;
            }
        }

        // Check for each receiver if we have a direct or indirect 
        // connection to it
        ConnectionInterface* nextReceiver = getConnectionToAgent(rit->getName());
        if(nextReceiver)
        {
            if(!nextReceiver->send(letter, fipa::acl::representation::BITEFFICIENT))
            {
                RTT::log(RTT::Error) << "MessageTransportService: message forwarding to mts '" << nextReceiver->getReceiverName() << "' failed" << RTT::endlog();
                return false;
            } else {
                RTT::log(RTT::Debug) << "MessageTransportService: message forwarding to mts '" << nextReceiver->getReceiverName() << "' succeeded" << RTT::endlog();
                continue;
            }
        } else {
            return false;
        }
    }

    return trigger();
}

ConnectionInterface* Task::getConnectionToAgent(const std::string& name) const
{
    boost::shared_lock<boost::shared_mutex> lock(mConnectionsMutex);

    std::map<std::string, ConnectionInterface*>::const_iterator it = mConnections.begin();
    for(;it != mConnections.end(); ++it)
    {
        if(!it->second)
        {
            RTT::log(RTT::Info) << "MessageTransportService: getConnectionToAgent: connection does not exist for agent '" << name << "'" << RTT::endlog();
            continue;
        }

        if(it->second->isConnected() && it->second->isAssociatedAgent(name))
        {
            return it->second;
        } else {
            RTT::log(RTT::Debug) << "MessageTransportService: getContentToAgent: agent '" << name << "' is not associated with mts '" << it->second << RTT::endlog();
        }
        
    }
    return NULL;
}

////////////////////////////////CALLBACKS///////////////////////////
void Task::serviceAdded_(std::string& remote_id, std::string& remote_ior, uint32_t buffer_size)
{
    CorbaConnection* cc = new CorbaConnection(this, remote_id, remote_ior, buffer_size);
    // do not connect here, but in update hook
    //cc->connect();

    mConnections.insert(std::pair<std::string, CorbaConnection*>(remote_id,cc));
}

void Task::serviceRemoved_(std::string& remote_id, std::string& remote_ior)
{
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
Task::Task() : TaskBase("mts::Task")
{
}

void Task::serviceAdded(sd::ServiceEvent se)
{
    std::string remote_id = se.getServiceConfiguration().getName();
    std::string remote_ior = se.getServiceConfiguration().getDescription("IOR");
    RTT::log(RTT::Info) << "MessageTransportService: New module " << remote_id << " addEvent" << RTT::endlog();

    if(remote_id == this->getName())
    {
        RTT::log(RTT::Info) << "MessageTransportService: 'new' module is self, thus not added." << RTT::endlog();
        return;
    }

    // Do nothing if the connection has already been established.
    {
        boost::unique_lock<boost::shared_mutex> lock(mConnectionsMutex);
        // If this is a duplicate this has to be handled in serviceAdded_
        try {
            serviceAdded_(remote_id, remote_ior, mConnectionBufferSize );
        } catch(const ConnectionException& e)
        {
            RTT::log(RTT::Info) << "MessageTransportService: ConnectionException: '" << e.what() << "'" << RTT::endlog();
            return;
        } catch(const InvalidService& e)
        {
            RTT::log(RTT::Info) << "MessageTransportService: Service '" << remote_id << "' is not an MessageTransportService: '" << e.what() << "'" << RTT::endlog();
            return;
        }
    }
    trigger();
}

void Task::serviceRemoved(sd::ServiceEvent se)
{
    std::string remote_id = se.getServiceConfiguration().getName();
    std::string remote_ior = se.getServiceConfiguration().getDescription("IOR");
    RTT::log(RTT::Info) << "MessageTransportService: Service '" << remote_id.c_str() << "' removed" << RTT::endlog();

    if(remote_id == this->getName())
    {
        return;
    }

    {
        boost::unique_lock<boost::shared_mutex> lock(mConnectionsMutex);
        std::map<std::string, ConnectionInterface*>::iterator it = mConnections.find(remote_id);
        if(it != mConnections.end()) // Connection to 'remote_id' available.
        {
            // Disconnect and delete.
            RTT::log(RTT::Info) << "MessageTransportService: Disconnecting from '" << remote_id.c_str() << "." << RTT::endlog();
            if(!it->second->disconnect())
            {
                RTT::log(RTT::Warning) << "MessageTransportService: Could not properly disconnect from '" << remote_id << ". Still performing cleanup.";
            }
            delete it->second;
            it->second = NULL;
            mConnections.erase(it);
        }

        serviceRemoved_(remote_id, remote_ior);
    }
}

void Task::cleanupHook()
{
    TaskBase::cleanupHook();

    delete mMessageTransport;
    mMessageTransport = NULL;

    delete mServiceDiscovery;
    mServiceDiscovery = NULL;

    // Delete all the remote mConnections.
    std::map<std::string, ConnectionInterface*>::iterator it;
    for(it=mConnections.begin(); it != mConnections.end(); it++)
    {
        it->second->disconnect();
        delete(it->second);
        it->second = NULL;
    }
    mConnections.clear();
}


////////////////////////////////RPC-METHODS//////////////////////////
std::vector<std::string> Task::getReceivers()
{
    ReceiverPorts::const_iterator cit = mReceivers.begin();
    std::vector<std::string> receivers;
    for(; cit != mReceivers.end(); ++cit)
    {
        receivers.push_back(cit->first);
    }
    return receivers;
}

bool Task::addReceiver(::std::string const & receiver)
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
        //triggerRemoteAgentListUpdate();
    }

    return success;
    
}


bool Task::removeReceiver(::std::string const & receiver)
{
        if(removeReceiverPort(receiver))
        {
            triggerRemoteAgentListUpdate();
            return true;
        }

        return false;
}

bool Task::addReceiverPort(RTT::base::OutputPortInterface* outputPort, const std::string& receiver)
{
    ports()->addPort(outputPort->getName(), *outputPort);
    mReceivers[receiver] = outputPort;
    RTT::log(RTT::Debug) << "MessageTransportService: Receiver port '" << receiver << "' added" << RTT::endlog();
    return true;
}

bool Task::removeReceiverPort(const std::string& receiver)
{
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

bool Task::updateAgentList(const std::string& mtsName, const std::vector<std::string>& agentList)
{
    boost::shared_lock<boost::shared_mutex> lock(mConnectionsMutex);
    std::map<std::string, ConnectionInterface*>::iterator it = mConnections.find(mtsName);
    if(it == mConnections.end())
    {
        RTT::log(RTT::Warning) << "MessageTransportService: updateAgentList failed, since mts '" << mtsName << "' is not known to this mts" << RTT::endlog();
        return false;
    } else {
        it->second->setAssociatedAgents(agentList);
        std::vector<std::string>::const_iterator it = agentList.begin();
        std::string agentListString;
        for(; it != agentList.end(); ++it)
        {
            agentListString += *it;
            agentListString += ",";
        }

        RTT::log(RTT::Info) << "Updated agent list: mts: '" << mtsName << "' with associated agents: '" << agentListString << "'" << RTT::endlog();
        return true;
    }
}

void Task::triggerRemoteAgentListUpdate(const std::string& mts)
{
    // Retrieve current list of receivers and send this information 
    // to all associated MTS
    std::vector<std::string> receivers = getReceivers();

    boost::shared_lock<boost::shared_mutex> lock(mConnectionsMutex);
    std::map<std::string, ConnectionInterface*>::iterator it = mConnections.begin();
    for(; it != mConnections.end(); ++it)
    {
        if(!mts.empty())
        {
            if(mts == it->first)
            {
                it->second->updateAgentList(receivers);
                return;
            }
        } else {
            it->second->updateAgentList(receivers);
        }
    }
}

} // namespace mts

