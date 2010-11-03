#include "Module.hpp"

#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <message-generator/ACLMessageOutputParser.h>
#include <rtt/corba/ControlTaskProxy.hpp>
#include <rtt/corba/ControlTaskServer.hpp>
#include <rtt/NonPeriodicActivity.hpp>

#include "TypelibMarshallerBase.hpp"

#include "module_id.h"
#include "connections/corba_connection.h"

namespace dc = dfki::communication;
namespace rc = RTT::Corba;

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
Module::Module(std::string const& name, 
            std::string const& conf_file) : ModuleBase(name),
        fipa(),
        connections(),
        conf(),
        propertyBag(NULL),
        loggerNames(),
        mta(NULL),
        serviceDiscovery(NULL),
        transport(NULL),
        configurationFilePath(conf_file),
        modID(name),
        periodicActivity(0.01),
        semaphoreConnect(NULL)
{
    conf = dc::ServiceConfiguration(name, "_rimres._tcp");
    semaphoreConnect = new sem_t();
    sem_init(semaphoreConnect, 1, 1);
    // See 'configureHook()'.
}

Module::~Module()
{
    if(serviceDiscovery != NULL)
    {
        delete serviceDiscovery;
        serviceDiscovery = NULL;
    }
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
    if(semaphoreConnect)
    {
        delete semaphoreConnect;
        semaphoreConnect = NULL;
    }
    stop();
    // See 'cleanupHook()'.
}

/*
RemoteConnection* Module::connectToModule(dc::ServiceEvent& se)
{
    // Do nothing if the connection have already be established.
    if(remoteConnectionsMap.find(se.getServiceDescription().getName()) != remoteConnectionsMap.end())
    {
        globalLog(RTT::Warning, "Connection to '%s' already established",
                se.getServiceDescription().getName().c_str());
        return NULL;
    }
    // Do not connect to yourself.
    if(se.getServiceDescription().getName() == conf.getName())
    {
        globalLog(RTT::Warning, "Modules are not allowed to connect to themselves.");
        return NULL;
    }
    // Create and add new connection.
    RemoteConnection* rm = new RemoteConnection(this, se);
    if(!rm->initialize())
    {
        globalLog(RTT::Error, "Connection to '%s' could not be initialized", 
                se.getServiceDescription().getName().c_str()); 
        delete rm; rm = NULL;
        return NULL;
    }
    report((RTT::InputPortInterface*)rm->getInputPort());
    // Use remote method to create needed ports on the remote module.
    RTT::Corba::ControlTaskProxy* ctp = rm->getControlTaskProxy();
    RTT::Method <bool(std::string const &, std::string const &)> create_remote_ports = 
            ctp->methods()->getMethod<bool(std::string const&, std::string const &)>("rpcConnectToModule");
    // Remote Methods are ready?
    if(!create_remote_ports.ready())
    {
        globalLog(RTT::Error, "Connection on the remote module '%s' could not be created", 
                se.getServiceDescription().getName().c_str());
        delete rm; rm = NULL;
        return NULL;
    }
    // Create remote connection.
    if(!create_remote_ports(rm->getLocalModuleName(), rm->getLocalIOR()))
    {
        globalLog(RTT::Error, "Connection on the remote module '%s' could not be created", 
                se.getServiceDescription().getName().c_str());
        delete rm; rm = NULL;
        return NULL;
    }

    // Refresh control task proxy.
    if(!rm->syncControlTaskProxy())
    {
        globalLog(RTT::Error, "Connection to '%s' could not be reestablished", 
                se.getServiceDescription().getName().c_str()); 
        delete rm; rm = NULL;
        return NULL;
    }
    // Connect local output to remote input port.
    ctp = rm->getControlTaskProxy();
    RTT::InputPortInterface* remoteinputport = NULL;
    remoteinputport = (RTT::InputPortInterface*)ctp->ports()->
            getPort(rm->getOutputPortName());
    if(remoteinputport == NULL)
    {
        globalLog(RTT::Error, "Inputport '%s' of the remote module '%s' could not be resolved",
            rm->getOutputPortName().c_str(), se.getServiceDescription().getName().c_str());
        return NULL;
    }

    int size_of_buffer = 2;
    
    //if(!rm->getOutputPort()->createBufferConnection(*remoteinputport, 
    //        size_of_buffer, 
    //        RTT::ConnPolicy::LOCK_FREE))
    //{
    

    // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, 
    // true=pull(problem here) false=push)
    if(!rm->getOutputPort()->connectTo(*remoteinputport, 
            RTT::ConnPolicy::buffer(20, RTT::ConnPolicy::LOCKED, false, false)))
    {
        globalLog(RTT::Error, "Outputport '%s' cant be connected to the Input port of %s",
            rm->getOutputPortName().c_str(), se.getServiceDescription().getName().c_str());
        delete rm; rm = NULL;
        return NULL;
    }
    globalLog(RTT::Info, "Connected to '%s'", se.getServiceDescription().getName().c_str());
    // Add to map of valid connections.
    remoteConnectionsMap.insert(std::pair<std::string, RemoteConnection*>
            (se.getServiceDescription().getName(), rm));
    return rm;
} 
    */

/*
void Module::disconnectFromModule(dc::ServiceEvent se)
{    
    std::string mod_name = se.getServiceDescription().getName();
    map<std::string, RemoteConnection*>::iterator it;
    it=remoteConnectionsMap.find(mod_name);
    if(it != remoteConnectionsMap.end()) { // found
        // Ports and control task proxy will be removed.
        delete(it->second); it->second = NULL;
        remoteConnectionsMap.erase(it);
        globalLog(RTT::Info, "Disconnected from %s", se.getServiceDescription().getName().c_str());
    } else {
        globalLog(RTT::Warning, "Connection to '%s' unknown, can not disconnect",
            se.getServiceDescription().getName().c_str());
    }
}
*/


/*
//bool Module::sendMessage(std::string const& receiver, boost::shared_ptr<fipa::BitefficientMessage> msg)
bool Module::sendMessage(std::string const& receiver, fipa::BitefficientMessage msg)
{
    map<std::string, RemoteConnection*>::iterator it = remoteConnectionsMap.find(receiver);
    // Receiver known?
    if(it != remoteConnectionsMap.end())
    {
        // Output ports available?
        RTT::OutputPort<fipa::BitefficientMessage>* output_port = it->second->getOutputPort();
        if(output_port) {
            // Convert message string to struct fipa::BitefficientMessage (has to be done because '\0'
            // within the string interupts sending)
            output_port->write(msg);
            return true;
        } else {
            globalLog(RTT::Warning, "No output ports available for receiver %s, message could not be sent",
                receiver.c_str());
            return false;
        }
    } else {
        globalLog(RTT::Warning, "Receiver %s unknown, message could not be sent",
                receiver.c_str());
        return false;
    }
}

bool Module::sendMessage(std::string const& receiver, std::string const& msg)
{
    // Creates the fipa::BitefficientMessage containing the message string on the heap.
//    boost::shared_ptr<fipa::BitefficientMessage> msg_vec(new fipa::BitefficientMessage(msg));
    fipa::BitefficientMessage msg_vec;
    return sendMessage(receiver, msg_vec);
}

//bool Module::sendMessageToMTA(boost::shared_ptr<fipa::BitefficientMessage> msg)
bool Module::sendMessageToMTA(fipa::BitefficientMessage msg)
{
    if(mts)
    {
        RTT::OutputPort<fipa::BitefficientMessage>* output_port = mts->getOutputPort();
        if(output_port) 
        {
            output_port->write(msg);
            return true;
        }
    }
    return false;
}  
*/

////////////////////////////////HOOKS///////////////////////////////
bool Module::configureHook()
{    
    if (RTT::log().getLogLevel() < RTT::Logger::Info)
    {
        RTT::log().setLogLevel( RTT::Logger::Info );
    }
    // SD has to be started here, constuctor does not work.
    configureModule();
    startServiceDiscovery();
    return true;
}

void Module::errorHook()
{
    globalLog(RTT::Error, "Entering error state.");
}

bool Module::startHook()
{
    // start SD
    return true;
}

void Module::updateHook(std::vector<RTT::PortInterface*> const& updated_ports)
{
    std::vector<RTT::PortInterface*>::const_iterator it;
    // Process message of all updated ports.
    for(it = updated_ports.begin(); it != updated_ports.end(); ++it)
    { 
//        boost::shared_ptr<fipa::BitefficientMessage> message(new fipa::BitefficientMessage());
        fipa::BitefficientMessage message;
        RTT::InputPortInterface* read_port = dynamic_cast<RTT::InputPortInterface*>(*it);
        ((RTT::InputPort<fipa::BitefficientMessage>*)read_port)->read(message);
        globalLog(RTT::Info, "Received new message on port %s of size %d", (*it)->getName().c_str(),
                message.size());
        std::string msg_str = message.toString();
        processMessage(msg_str);
    }
}

////////////////////////////////////////////////////////////////////
//                           PROTECTED                            //
////////////////////////////////////////////////////////////////////
void Module::configureModule(std::string const& file)
{
    // Set default values. Random Name for the module.
    char buffer[sizeof(int)*8+1];
    srand(time(NULL));
    sprintf(buffer, "%d", rand() % 100000);

    conf.setName("A_ROOT_DEFAULT" + std::string(buffer));
    conf.setType("_rimres._tcp");
    conf.setPort(12000);
    conf.setTTL(0);
    //conf.setRawDescriptions(RTT::Corba::ControlTaskServer::getIOR(this));
    conf.setDescription("IOR", RTT::Corba::ControlTaskServer::getIOR(this));
    // Try to load the config-file.
    std::string module_path = "";
    if (configuration_file != "")
	    module_path = configuration_file;
    else 
	    module_path = "../configuration/module.xml";
    if(!this->marshalling()->loadProperties(module_path))
    {
        globalLog(RTT::Warning, "Could not load properties %s, default values are used",
            module_path.c_str());
    } else {
        propertyBag = this->properties(); // For external usage.
        RTT::PropertyBag* pb = propertyBag;
        // Returns NULL if the property could not be loaded. 
        RTT::Property<std::string>* p_name = pb->getProperty<std::string>("name");
        RTT::Property<std::string>* p_type = pb->getProperty<std::string>("avahi_type");
        RTT::Property<int>* p_port = pb->getProperty<int>("port");
        RTT::Property<int>* p_ttl = pb->getProperty<int>("ttl");
        RTT::Property<double>* p_activity = pb->getProperty<double>("periodic_activity");

        if(p_name) conf.setName(p_name->get());
        if(p_type) conf.setType(p_type->get());
        if(p_port) conf.setPort(p_port->get());
        if(p_ttl)  conf.setTTL(p_ttl->get());
        if(p_activity) this->periodicActivity = p_activity->get();
    }

    // Set name of this task context.
    this->setName(conf.getName());

    // Split and store module ID to envID, avahi type and name.
    modID = ModuleID(conf.getName());

    // Getting information for the type of the ports (fipa::BitefficientMessage)
    RTT::TypeInfo const* type = _inputPortMTS.getTypeInfo();
    transport = dynamic_cast<orogen_transports::TypelibMarshallerBase*>(
        type->getProtocol(orogen_transports::TYPELIB_MARSHALLER_ID));
    if (! transport)
    {
        // TODO change the log
        log(RTT::Error) << "cannot report ports of type " << type->getTypeName()
         << " as no toolkit generated by orogen defines it" << RTT::endlog();
    }    
}

/*boost::shared_ptr<fipa::BitefficientMessage> Module::generateMessage(const std::string& content, 
        const std::set<std::string>& receivers)
{
    return generateMessage(content, std::string(this->getName()), receivers);	
}

boost::shared_ptr<fipa::BitefficientMessage> Module::generateMessage(const std::string& content, 
        const std::string sender,
        const std::set<std::string>& receivers)
{
    // Build fipa message.
    fipa::acl::ACLMessage message = fipa::acl::ACLMessage(std::string("query-ref"));
    message.setContent(std::string(content));

    fipa::acl::AgentID fipa_sender = fipa::acl::AgentID(sender);
    message.setSender(fipa_sender);

    for (std::set<std::string>::const_iterator it = receivers.begin(); it != receivers.end(); ++it) {
        fipa::acl::AgentID receiver = fipa::acl::AgentID(std::string((*it)));
        message.addReceiver(receiver);
    }

    // Generate bit message.
    fipa::acl::ACLMessageOutputParser generator = fipa::acl::ACLMessageOutputParser();
    generator.setMessage(message);

    boost::shared_ptr<fipa::BitefficientMessage> bytemessage(new fipa::BitefficientMessage());

    bytemessage->push_back(generator.getBitMessage());

    return bytemessage;	
}
*/
/*
fipa::BitefficientMessage Module::generateMessage(const std::string& content, 
        const std::set<std::string>& receivers)
{
    return generateMessage(content, std::string(this->getName()), receivers);	
}

fipa::BitefficientMessage Module::generateMessage(const std::string& content, 
        const std::string sender,
        const std::set<std::string>& receivers)
{
    // Build fipa message.
    fipa::acl::ACLMessage message = fipa::acl::ACLMessage(std::string("query-ref"));
    message.setContent(std::string(content));

    fipa::acl::AgentID fipa_sender = fipa::acl::AgentID(sender);
    message.setSender(fipa_sender);

    for (std::set<std::string>::const_iterator it = receivers.begin(); it != receivers.end(); ++it) {
        fipa::acl::AgentID receiver = fipa::acl::AgentID(std::string((*it)));
        message.addReceiver(receiver);
    }

    // Generate bit message.
    fipa::acl::ACLMessageOutputParser generator = fipa::acl::ACLMessageOutputParser();
    generator.setMessage(message);

    fipa::BitefficientMessage bytemessage;

    bytemessage.push_back(generator.getBitMessage());

    return bytemessage;	
}

fipa::BitefficientMessage Module::generateMessage(const std::string& content, 
        const std::string sender,
        const std::set<std::string>& receivers,
        const std::string& conversation_id)
{
    // Build fipa message.
    fipa::acl::ACLMessage message = fipa::acl::ACLMessage(std::string("query-ref"));
    message.setContent(std::string(content));

    fipa::acl::AgentID fipa_sender = fipa::acl::AgentID(sender);
    message.setSender(fipa_sender);

    message.setConversationID(conversation_id);

    for (std::set<std::string>::const_iterator it = receivers.begin(); it != receivers.end(); ++it) {
        fipa::acl::AgentID receiver = fipa::acl::AgentID(std::string((*it)));
        message.addReceiver(receiver);
    }

    // Generate bit message.
    fipa::acl::ACLMessageOutputParser generator = fipa::acl::ACLMessageOutputParser();
    generator.setMessage(message);

    fipa::BitefficientMessage bytemessage;

    bytemessage.push_back(generator.getBitMessage());

    return bytemessage;	
}
*/

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
            } catch (MessageException& e) {
                log(RTT::Warning) << "MessageException: " << e.what() << RTT::endlog();
            } catch (ConnectionException& e) {
                log(RTT::Warning) << "ConnectionException: " << e.what() << RTT::endlog();
            }
        }
    }
    // Local log.
    log(log_type) << msg << RTT::endlog();
}

void Module::startServiceDiscovery()
{
    if(serviceDiscovery == NULL)
    {
        serviceDiscovery = new dc::ServiceDiscovery();
        // conf.stringlist.push_back("Type=Basis");
        serviceDiscovery->addedComponentConnect(sigc::mem_fun(*this, 
            &Module::serviceAdded));
        serviceDiscovery->removedComponentConnect(sigc::mem_fun(*this, 
            &Module::serviceRemoved));

    }
    try{
        serviceDiscovery->start(conf);
    } catch(exception& e) {
        globalLog(RTT::Error, "%s", e.what());
    }
    globalLog(RTT::Info, "Started service '%s' with avahi-type '%s'",
        this->getName().c_str(), conf.getType().c_str());
}

////////////////////////////////CALLBACKS///////////////////////////
void Module::serviceAdded_(dfki::communication::ServiceEvent& se)
{
    std::string id = se.getServiceDescription().getName();

    std::map<std::string, ConnectionInterface*>::iterator it;
    it = connections.find(id);

    if(it == connections.end())
    {
        log(RTT::Info) << "Connection to " << id << " already established." 
                << RTT::endlog();
        return;
    }

    ModuleID mod(id);
    std::string remoteIOR = se.getServiceDescription().getDescription("IOR");

    // Connect to the first appropriate MTA (same environment ids).  
    if(mta == NULL && (mod.getType() == "MTA" && mod.getEnvID() == this->modID.getEnvID()))
    {
        CorbaConnection* cc = new CorbaConnection(this, id, remoteIOR);
        try{
            cc->connect();
        } catch(ConnectionException& e) {
            log(RTT::Info) << "ConnectionException: " << e.what() << RTT::endlog();
            return;        
        }
        connections.insert(pair<std::string, CorbaConnection*>(id,cc));
    }

    // Build up a list with all the logging-module-IDs.  
    if(mod.getType() == "LOG")
    {
        loggerNames.push_back(id);
    }
}

void Module::serviceRemoved_(dfki::communication::ServiceEvent& se)
{
    // Do nothing if no connection is available.
    std::string id = se.getServiceDescription().getName();
    std::map<std::string, ConnectionInterface*>::iterator it = connections.find(id);
    if(it == connections.end()) // No connection to 'id' available.
        return;

    ModuleID mod(id);

    // If its the MTA of this module, remove shortcut.
    if(mod.getType() == "MTA" && mod.getEnvID() == this->modID.getEnvID())
    {
        mta = NULL;
        globalLog(RTT::Warning, "My MTA has been removed.");
    } 

    // If its a logging module, remove entry in the logger list.
    if(mod.getType() == "LOG")
    {
        std::vector<std::string>::iterator it=loggerNames.begin();
        for(; it!=loggerNames.end(); ++it)
        {
            if(*it == id)
            {
                loggerNames.erase(it);
                break;
            }
        }
    }

    // Disconnect and delete.
    it->second->disconnect();
    connections.erase(it);
}

////////////////////////////////METHODS////////////////////////////
bool Module::rpcCreateConnectPorts(std::string const& remote_name, 
        std::string const& remote_ior)
{
    sem_wait(semaphoreConnect);
    // Do nothing if the connection have already be established.
    if(connections.find(remote_name) != connections.end())
    {
        globalLog(RTT::Warning, "Connection to '%s' already established", 
            remote_name.c_str());
        sem_post(semaphoreConnect);
        return true; // Returns true because connection is available.
    }
    
    // Create the ports and the proxy and use the proxy to connect 
    // the local output to the remote input.
    CorbaConnection* con = new CorbaConnection(this, remote_name, remote_ior);
    try {
        con->connectLocal();
    } catch(ConnectionException& e) {
        globalLog(RTT::Error, "Connection to '%s' could not be established.", remote_name.c_str());
        return false;
    }

    connections.insert(pair<std::string, CorbaConnection*>(remote_name,con));
    globalLog(RTT::Info, "Connected to '%s'", remote_name.c_str());
    /*
    // Create and add new connection.
    RemoteConnection* rm =  new RemoteConnection(this, remote_name, remote_ior);
    if(!rm->initialize())
    {
        globalLog(RTT::Error, "Connection to '%s' could not be initialized", 
            remote_name.c_str());        
        delete rm; rm = NULL;
        sem_post(semaphoreConnect);
        return false;
    }  
    // Connect the local output to the remote input.
    // The remote input port has the same name as the local output port 
    // (localmodule->remotemodule_port)).
    RTT::Corba::ControlTaskProxy* ctp = rm->getControlTaskProxy();
    RTT::InputPortInterface* remoteinputport = NULL;
    remoteinputport = (RTT::InputPortInterface*)ctp->ports()->
            getPort(rm->getOutputPortName());
    if(remoteinputport == 0)
    {
        globalLog(RTT::Error, "Inputport '%s' of the remote module '%s' could not be resolved",
            rm->getOutputPortName().c_str(), remote_name.c_str());
        delete rm; rm = NULL;
        sem_post(semaphoreConnect);
        return false;
    }

    int size_of_buffer = 2;
    
    //if(!rm->getOutputPort()->createBufferConnection(*remoteinputport, 
    //        size_of_buffer, 
    //        RTT::ConnPolicy::LOCK_FREE))
    //{
    
    //if(!rm->getOutputPort()->connectTo(*remoteinputport, 
    //    ((RTT::InputPortInterface*)remoteinputport)->getDefaultPolicy()))
    //{
    // int size, int lock_policy = LOCK_FREE, bool init_connection = false, bool pull = false
    if(!rm->getOutputPort()->connectTo(*remoteinputport, RTT::ConnPolicy::buffer(20, RTT::ConnPolicy::LOCKED, false, false)))
    {
        globalLog(RTT::Error, "Outputport '%s' cant be connected to Inputport of '%s'",
            rm->getOutputPortName().c_str(), remote_name.c_str());
        delete rm; rm = NULL;
        sem_post(semaphoreConnect);
        return false;
    }
    */

    //globalLog(RTT::Info, "Connected to '%s'", remote_name.c_str());
    //remoteConnectionsMap.insert(std::pair<std::string, RemoteConnection*>(remote_name, rm));
    //sem_post(semaphoreConnect);
    return true;
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
void Module::serviceAdded(dfki::communication::ServiceEvent se)
{
    serviceAdded_(se);
}

void Module::serviceRemoved(dfki::communication::ServiceEvent se)
{
    serviceRemoved_(se);
}
} // namespace modules

