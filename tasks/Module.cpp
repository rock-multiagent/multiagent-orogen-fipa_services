#include "Module.hpp"

#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <fipa_acl/bitefficient_message.h> 
#include <rtt/transports/corba/TaskContextServer.hpp>

#include "module_id.h"

namespace sd = servicediscovery;
namespace rc = RTT::corba;

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
Module::Module(std::string const& name) : ModuleBase(name),
        fipa(),
        connections(),
        mta(NULL),
        loggerNames(),
        serviceDiscovery(NULL),
        modID(name),
        connectSem(NULL),
        modifyModuleListSem(NULL),
        removeConnectionsSem(NULL)
{
    connectSem = new sem_t();
    if( 0 != sem_init(connectSem, 1, 1))
    {
        RTT::log(RTT::Warning) << "Semaphore for protecting connection of multiple modules could not be initialized" << RTT::endlog();
    }

    modifyModuleListSem = new sem_t();
    if( 0 != sem_init(modifyModuleListSem, 1 /*1: shared between processes, 0: shared been threads of a process */, 1 /*initial value of the semaphore*/))
    {
        RTT::log(RTT::Warning) << "Semaphore for modifying list could not be initialized" << RTT::endlog();
    }

    removeConnectionsSem = new sem_t();
    if( 0 != sem_init(removeConnectionsSem, 1 /*1: shared between processes, 0: shared been threads of a process */, 1 /*initial value of the semaphore*/))
    {
        RTT::log(RTT::Warning) << "Semaphore for removing connections could not be initialized" << RTT::endlog();
    }

}

Module::Module(std::string const& name, RTT::ExecutionEngine* engine) : ModuleBase(name, engine), 
        fipa(),
        connections(),
        mta(NULL),
        loggerNames(),
        serviceDiscovery(NULL),
        modID(name),
        connectSem(NULL),
        modifyModuleListSem(NULL),
        removeConnectionsSem(NULL)
{
    connectSem = new sem_t();
    if( ! sem_init(connectSem, 1, 1) )
    {
        RTT::log(RTT::Warning) << "Semaphore for protecting connection of multiple modules could not be initialized" << RTT::endlog();
    }

    modifyModuleListSem = new sem_t();
    if( ! sem_init(modifyModuleListSem, 0 /*1: shared between processes, 0: shared been threads of a process */, 1 /*initial value of the semaphore*/))
    {
        RTT::log(RTT::Warning) << "Semaphore for modifying list could not be initialized" << RTT::endlog();
    }

    removeConnectionsSem = new sem_t();
    if( 0 != sem_init(removeConnectionsSem, 1 /*1: shared between processes, 0: shared been threads of a process */, 1 /*initial value of the semaphore*/))
    {
        RTT::log(RTT::Warning) << "Semaphore for removing connections could not be initialized" << RTT::endlog();
    }
}

Module::~Module()
{
    delete serviceDiscovery;
    serviceDiscovery = NULL;

    // Delete all the remote connections.
    std::map<std::string, ConnectionInterface*>::iterator it;
    for(it=connections.begin(); it != connections.end(); it++)
    {
        it->second->disconnect();
        delete(it->second);
        it->second = NULL;
    }
    connections.clear();
    // The mts-connection has been deleted as well.
    mta = NULL;
    delete connectSem;
    connectSem = NULL;

    delete modifyModuleListSem;
    delete removeConnectionsSem;

    stop();
    // See 'cleanupHook()'.
}

////////////////////////////////HOOKS///////////////////////////////
bool Module::configureHook()
{
    // Set logger info.
    if (RTT::log().getLogLevel() < RTT::Logger::Info)
    {
        RTT::log().setLogLevel( RTT::Logger::Info );
    }

    // If configure properties are empty, module will be stopped.
    if(_avahi_type.get().empty() || _avahi_port.get() == 0)
    {
        globalLog(RTT::Info, "Root: Properties are not set, module will be stopped.");
        return false;
    }

    // NOTE: Setting of names of TaskContext do not work in RTT 1.X and in RTT 2.0
    // We are using the deployment name instead, i.e. the deployment already provides
    // the service name, i.e. instance and id correspond

    // For disambiguation over multiple systems when using the same deployment
    // we use the system id

    // NOTE2: The deployment name is now set to: <FAMOS_SYSTEM_ID>_<NAME> during build.
    // E.g.: sherpa_0_ROOT01 or sherpa_0_MTA. The avahi name equivalent.
    /*
    char* systemId = getenv("FAMOS_SYSTEM_ID");
    std::string systemIdString(systemId);
    if(systemIdString != "")
	    systemIdString = this->getName()+ "_" + systemIdString;
    else
	    systemIdString = this->getName();
    */	  
    modID = ModuleID(this->getName());

    // Configure SD.
    sd::ServiceConfiguration sc(this->getName(), _avahi_type.get(), _avahi_port.get());
    sc.setTTL(_avahi_ttl.get());
    sc.setDescription("IOR", rc::TaskContextServer::getIOR(this));
    serviceDiscovery = new sd::ServiceDiscovery();

    // Add callback functions.
    serviceDiscovery->addedComponentConnect(sigc::mem_fun(*this, 
        &Module::serviceAdded));
    serviceDiscovery->removedComponentConnect(sigc::mem_fun(*this, 
        &Module::serviceRemoved));
    // Start SD.
    try{
        serviceDiscovery->start(sc);
    } catch(const std::exception& e) {
        globalLog(RTT::Error, "%s", e.what());
    }
    globalLog(RTT::Info, "Root: Started service '%s'. Avahi-type: '%s'. Port: %d. TTL: %d.", 
        modID.getID().c_str(), _avahi_type.get().c_str(), _avahi_port.get(), _avahi_ttl.get());

    mConnectionBufferSize = _connection_buffer_size.get();

    return true;
}

void Module::errorHook()
{
    globalLog(RTT::Error, "Root: Entering error state.");
}

bool Module::startHook()
{
    return true;
}

void Module::stopHook()
{
}

void Module::updateHook()
{
    sem_wait(removeConnectionsSem);
    const RTT::DataFlowInterface::Ports& ports = this->ports()->getPorts();

    for(RTT::DataFlowInterface::Ports::const_iterator it = ports.begin(); it != ports.end(); it++)
    { 
		RTT::InputPort<fipa::BitefficientMessage>* msg_port = dynamic_cast< RTT::InputPort<fipa::BitefficientMessage>* >(*it);
		fipa::BitefficientMessage message;
		if(msg_port)
		{
                        std::string portName = msg_port->getName();
                        uint32_t count = 0;
			while( msg_port->read(message) == RTT::NewData)
			{
				globalLog(RTT::Info, "Root: Received new message on port %s of size %d",
						portName.c_str(), message.size());
				std::string msg_str = message.toString();
				processMessage(msg_str);
                                ++count;
			}
                        mPortStats[portName].update(count);
		}
    }
    std::map<std::string, base::Stats<double> >::iterator statsIt = mPortStats.begin();
    std::vector<root::PortStats> portStats;
    for(; statsIt != mPortStats.end(); ++statsIt)
    {
        portStats.push_back(root::PortStats(statsIt->first, statsIt->second));
    }
    sem_post(removeConnectionsSem);

    _port_stats.write(portStats);
}

////////////////////////////////////////////////////////////////////
//                           PROTECTED                            //
////////////////////////////////////////////////////////////////////
void Module::globalLog(RTT::LoggerLevel log_type, const char* format, ...)
{
    int n = 100;	
	char buffer[512];
	va_list arguments;

	va_start(arguments, format);
	n = vsnprintf(buffer, sizeof(buffer), format, arguments);
	va_end(arguments);
    std::string msg(buffer);

    // Global log, sending message to all log-modules.
    if(loggerNames.size()) // Are log-modules active?
    {
        // Build logging string with 'RTT::Logger/modname/msg'.
        std::string log_msg = "x/" + this->getName() + "/" + msg;
        log_msg[0] = static_cast<int>(log_type); // log_type 0-6

        if(mta)
        {
            // Create message
            try {
                fipa.clear();
                fipa.setMessage("SENDER "+ this->getName() + 
                        "CONTENT START " + log_msg + " STOP");
                fipa.setParameter("RECEIVER", loggerNames);
                mta->send(fipa.encode());
            } catch (const MessageException& e) {
                log(RTT::Warning) << "Root: MessageException: " << e.what() << RTT::endlog();
            } catch (const ConnectionException& e) {
                log(RTT::Warning) << "Root: ConnectionException: " << e.what() << RTT::endlog();
            }
        }
    }
    // Local log.
    log(log_type) << msg << RTT::endlog();
}

bool Module::isConnectedTo(std::string name)
{
    std::map<std::string, ConnectionInterface*>::iterator it;
    it = connections.find(name);
    return it != connections.end();
}

// Process message has to be overwritten by an inheriting
// module
bool Module::processMessage(std::string& message)
{
    // If not overwritten, just send all messages back to the sender.
    if(mta == NULL)
    {
        log(RTT::Warning) << "Root: No MTA available." << RTT::endlog();
        return false;
    }
    try
    {
        fipa.decode(message);
        std::vector<std::string>& content = fipa.getEntry("CONTENT");
        if(content.size())
            log(RTT::Info) << "Root: Received: " << content.at(0) << RTT::endlog();
        
        // Send answer if a sender has been defined.
        std::vector<std::string>& sender_vec = fipa.getEntry("SENDER");
        if(sender_vec.size())
        {
            std::string sender = sender_vec.at(0);
            std::cout << "NEW SENDER " << modID.getID() << " NEW RECEIVER " << sender << std::endl; 
            fipa.clear("RECEIVER SENDER");
            fipa.setMessage("RECEIVER "+sender);
            fipa.setMessage("SENDER "+modID.getID());
            std::string msg = fipa.encode();
            if(!mta->send(msg))
            {
                log(RTT::Warning) << "Failed to send message from '" << modID.getID() << " to " << sender << RTT::endlog();
            }
        }

    } catch(const ConnectionException& e)
    {
        log(RTT::Warning) << "Root: ConnectionException: " << e.what() << RTT::endlog();
        return false;
    } catch(const MessageException& e)
    {
        log(RTT::Warning) << "Root: MessageException: " << e.what() << RTT::endlog();
        return false;
    }
    return true;
}

bool Module::sendMessage(std::string sender_id, std::string recv_id, 
        std::string msg_content, std::string conversation_id)
{
    RTT::log(RTT::Debug) << "Root: send message sender_id '" << sender_id << "' recv_id '" << recv_id <<
        "' msg_content '" << msg_content << "' conversation_id '" << conversation_id << "'" << RTT::endlog();

    std::map<std::string, ConnectionInterface*>::iterator it;
    ConnectionInterface* recv_con = mta;

    // Check local receivers if no MTA is available
    std::string recv_name;
    if(recv_con == NULL)
    {
        if(!recv_id.empty())
        {
            ModuleID modID_(recv_id);

            // Check whether this is a valid receiver
            if(modID_.getType() == "MTA")
            {
                RTT::log(RTT::Error) << "Root: message receiver is set to '" << recv_id << "', but an MTA cannot serve as receiving endpoint" << RTT::endlog();
                return false;
            }

            // Same environment: send directly, else: foward message to MTA
            // of the enviroment (if known)
            if(modID.getEnvID() == modID_.getEnvID())
            {
                recv_name = recv_id;
            } else {
                recv_name = modID_.getEnvID()+"_MTA";
            }

            it = connections.find(recv_name);
            if(it != connections.end())
                recv_con = it->second;
        }
    }
    
    // If no MTA for the specified enviroment is known
    // return error
    if(recv_con == NULL)
    {
        log(RTT::Error) << "Root: No suitable MTA to '" << recv_name << "' found." << RTT::endlog();
        return false;
    }

    std::string msg;        
    try
    {
        // Create message.
        FipaMessage fipa_;
        fipa_.setMessage("SENDER "+sender_id);
        fipa_.setMessage("RECEIVER BEGIN " +recv_id+ " END");
        fipa_.setMessage("CONTENT BEGIN " +msg_content+ " END");
        fipa_.setMessage("CONVERSATION-ID " +conversation_id);
        fipa_.setMessage("PERFORMATIVE inform");
        msg = fipa_.encode();
    } catch (const MessageException& e) {
        log(RTT::Error) << "MessageException: " << e.what() << RTT::endlog();
        return false;
    }

    // Send via to local receiver or a responsible MTA
    return recv_con->send(msg);
}

////////////////////////////////RPC-METHODS//////////////////////////
bool Module::rpcCreateConnectPorts(std::string const& remote_name, 
        std::string const& remote_ior, boost::int32_t buffer_size)
{
    //sem_wait(connectSem);   
    // Return true if the connection have already be established.
    if(connections.find(remote_name) != connections.end())
    {
        globalLog(RTT::Warning, "Root: (RPC) Connection to '%s' already established", 
            remote_name.c_str());
        //sem_post(connectSem);
        return true;
    }
    
    // Create the ports and the proxy and use the proxy to connect 
    // the local output to the remote input.
    CorbaConnection* con = new CorbaConnection(this, remote_name, remote_ior, buffer_size);
    try {
        con->connectLocal();
    } catch(const ConnectionException& e) {
        globalLog(RTT::Error, "Root: (RPC) Connection to '%s' could not be established: %s", 
                remote_name.c_str(), e.what());
        //sem_post(connectSem);
        return false;
    }

    connections.insert(pair<std::string, CorbaConnection*>(remote_name,con));
    globalLog(RTT::Info, "Root: (RPC) Connected to '%s'", remote_name.c_str());
    //sem_post(connectSem);
    return true;
}

////////////////////////////////CALLBACKS///////////////////////////
void Module::serviceAdded_(std::string& remote_id, std::string& remote_ior, uint32_t buffer_size)
{
    ModuleID mod(remote_id);
    if(mod.getType() != "MTA")
    {
        RTT::log(RTT::Info) << "Module is no MTA" << RTT::endlog();
        return;
    }

    // Connect to the first appropriate MTA (same environment ids).  
    if(mta != NULL)
    {
        RTT::log(RTT::Info) << "MTA is already set for this component" << RTT::endlog();
        return;
    }
    
    if (mod.getEnvID() != this->modID.getEnvID())
    {
    	RTT::log(RTT::Info) << "WARNING: MTA for other environment than this (" << this->modID.getEnvID() << ") found: " << remote_id << RTT::endlog();
    	return;
    }

    RTT::log(RTT::Info) << "MTA found for my environment (" << this->modID.getEnvID() << ") found: " << remote_id << RTT::endlog();

    CorbaConnection* cc = new CorbaConnection(this, remote_id, remote_ior, buffer_size);
    try {
        cc->connect();
    } catch(const ConnectionException& e)
    {
        globalLog(RTT::Info, "Root: ConnectionException: %s", e.what());
        return;        
    }
    connections.insert(pair<std::string, CorbaConnection*>(remote_id,cc));
    mta = cc;
    globalLog(RTT::Info, "Root: Connected to %s.", remote_id.c_str());
    
}

void Module::serviceRemoved_(std::string& remote_id, std::string& remote_ior)
{
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
Module::Module() : ModuleBase("root::Module"), modID("root::Module")
{
}

void Module::serviceAdded(sd::ServiceEvent se)
{
    std::string remote_id = se.getServiceConfiguration().getName();
    std::string remote_ior = se.getServiceConfiguration().getDescription("IOR");
    ModuleID mod(remote_id);
    globalLog(RTT::Info, "Root: New module %s addEvent", remote_id.c_str());

    if(remote_id == this->getName())
    {
        globalLog(RTT::Info, "Root: 'new' module is self, thus not added.");
        return;
    }

    // Build up a list with all the logging-module-IDs.  
    if(mod.getType() == "LOG")
    {
        loggerNames.insert(remote_id);
    }

    // Do nothing if the connection has already been established.
    sem_wait(modifyModuleListSem);

    std::map<std::string, ConnectionInterface*>::iterator it;
    it = connections.find(remote_id);
    if(it != connections.end())
    {
        globalLog(RTT::Info, "Root: Connection to %s already established.",remote_id.c_str());
    	sem_post(modifyModuleListSem);
        return;
    }

    serviceAdded_(remote_id, remote_ior, mConnectionBufferSize );
    sem_post(modifyModuleListSem);
}

void Module::serviceRemoved(sd::ServiceEvent se)
{
    std::string remote_id = se.getServiceConfiguration().getName();
    std::string remote_ior = se.getServiceConfiguration().getDescription("IOR");
    ModuleID mod(remote_id);
    globalLog(RTT::Info, "Root: Module %s removed", remote_id.c_str());

    if(remote_id == this->getName())
    {
        return;
    }

    // If its a logging module, remove entry in the logger list.
    if(mod.getType() == "LOG")
    {
        loggerNames.erase(remote_id);
    }

    std::map<std::string, ConnectionInterface*>::iterator it = connections.find(remote_id);
    sem_wait(removeConnectionsSem);
    if(it != connections.end()) // Connection to 'remote_id' available.
    {
        // Disconnect and delete.
        globalLog(RTT::Info, "Root: Disconnecting from %s.", remote_id.c_str());
        if(!it->second->disconnect())
        {
            globalLog(RTT::Warning, "Root: Could not properly disconnect from %s. Still performing cleanup.", remote_id.c_str());
        }
        connections.erase(it);

        // If its the MTA of this module, remove shortcut.
        if(mod.getType() == "MTA" && mod.getEnvID() == this->modID.getEnvID())
        {
            mta = NULL;
            globalLog(RTT::Warning, "Root: My MTA has been removed.");
        }
    }

    serviceRemoved_(remote_id, remote_ior);
    sem_post(removeConnectionsSem);
}
} // namespace modules

