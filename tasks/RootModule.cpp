#include "RootModule.hpp"

#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <message-generator/ACLMessageOutputParser.h>
#include <message-parser/message_parser.h>
#include <rtt/corba/ControlTaskProxy.hpp>
#include <rtt/corba/ControlTaskServer.hpp>
#include <rtt/NonPeriodicActivity.hpp>

#include "TypelibMarshallerBase.hpp"

#include "ModuleID.hpp"
#include "RemoteConnection.hpp"

namespace dc = dfki::communication;
namespace rc = RTT::Corba;

int test_counter = 0;

namespace modules
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
RootModule::RootModule(std::string const& name, 
            std::string const& conf_file) : RootModuleBase(name),
        serviceDiscovery(NULL),
        mts(NULL),
        configuration_file(conf_file),
        semaphoreConnect(NULL),
        conf(),
        remoteConnectionsMap(),
        loggerSet(),
        periodicActivity(0.01),
        propertyBag(NULL)
{
    conf = dc::ServiceConfiguration(name, "_rimres._tcp");
    semaphoreConnect = new sem_t();
    sem_init(semaphoreConnect, 1, 1); // Shared between processes and value one.
    // See 'configureHook()'.
}

RootModule::~RootModule()
{
    if(serviceDiscovery != NULL)
    {
        delete serviceDiscovery;
        serviceDiscovery = NULL;
    }
    // Delete all the remote connections.
    std::map<std::string, RemoteConnection*>::iterator it;
    for(it=remoteConnectionsMap.begin(); it != remoteConnectionsMap.end(); it++)
    {
        delete(it->second);
        it->second = NULL;
    }
    remoteConnectionsMap.clear();
    // The mts-connection has been deleted as well.
    mts = NULL;
    if(semaphoreConnect)
    {
        delete semaphoreConnect;
        semaphoreConnect = NULL;
    }
    stop();
    // See 'cleanupHook()'.
}

//todo: add the rm to the map at the end, delete rm if connection fails.
RemoteConnection* RootModule::connectToModule(dc::ServiceEvent se)
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
    /*
    if(!rm->getOutputPort()->createBufferConnection(*remoteinputport, 
            size_of_buffer, 
            RTT::ConnPolicy::LOCK_FREE))
    {
    */

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

void RootModule::disconnectFromModule(dc::ServiceEvent se)
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

RTT::NonPeriodicActivity* RootModule::getNonPeriodicActivity()
{
    return dynamic_cast< RTT::NonPeriodicActivity* >(getActivity().get());
}

//bool RootModule::sendMessage(std::string const& receiver, boost::shared_ptr<fipa::BitefficientMessage> msg)
bool RootModule::sendMessage(std::string const& receiver, fipa::BitefficientMessage msg)
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

bool RootModule::sendMessage(std::string const& receiver, std::string const& msg)
{
    // Creates the fipa::BitefficientMessage containing the message string on the heap.
//    boost::shared_ptr<fipa::BitefficientMessage> msg_vec(new fipa::BitefficientMessage(msg));
    fipa::BitefficientMessage msg_vec;
    return sendMessage(receiver, msg_vec);
}

//bool RootModule::sendMessageToMTA(boost::shared_ptr<fipa::BitefficientMessage> msg)
bool RootModule::sendMessageToMTA(fipa::BitefficientMessage msg)
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

////////////////////////////////HOOKS///////////////////////////////
bool RootModule::configureHook()
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

void RootModule::errorHook()
{
    globalLog(RTT::Error, "Entering error state.");
}

bool RootModule::startHook()
{
    // start SD
    return true;
}

void RootModule::updateHook(std::vector<RTT::PortInterface*> const& updated_ports)
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
        processMessage(message);
    }
}

////////////////////////////////////////////////////////////////////
//                           PROTECTED                            //
////////////////////////////////////////////////////////////////////
void RootModule::configureModule()
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
    ModuleID::splitID(conf.getName(), &envID, &type, &name);

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

/*boost::shared_ptr<fipa::BitefficientMessage> RootModule::generateMessage(const std::string& content, 
        const std::set<std::string>& receivers)
{
    return generateMessage(content, std::string(this->getName()), receivers);	
}

boost::shared_ptr<fipa::BitefficientMessage> RootModule::generateMessage(const std::string& content, 
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

fipa::BitefficientMessage RootModule::generateMessage(const std::string& content, 
        const std::set<std::string>& receivers)
{
    return generateMessage(content, std::string(this->getName()), receivers);	
}

fipa::BitefficientMessage RootModule::generateMessage(const std::string& content, 
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

size_t RootModule::getPayloadSize() 
{
     return 1;
}

void RootModule::globalLog(RTT::LoggerLevel log_type, const char* format, ...)
{
    int n = 100;	
	char buffer[512];
	va_list arguments;

	va_start(arguments, format);
	n = vsnprintf(buffer, sizeof(buffer), format, arguments);
	va_end(arguments);
    std::string msg(buffer);

    // Global log, sending message to all log-modules.
    if(loggerSet.size()) // Are log-modules active?
    {
        // Build logging string with 'RTT::Logger/modname/msg'.
        std::string log_msg = "x/" + this->getName() + "/" + msg;
        log_msg[0] = static_cast<int>(log_type); // log_type 0-6

//        boost::shared_ptr<fipa::BitefficientMessage> fipa_msg = generateMessage(log_msg, loggerSet);
        fipa::BitefficientMessage fipa_msg = generateMessage(log_msg, loggerSet);

        sendMessageToMTA(fipa_msg);
    }
    
    // Local log.
    log(log_type) << msg << RTT::endlog();
}

void RootModule::startServiceDiscovery()
{
    if(serviceDiscovery == NULL)
    {
        serviceDiscovery = new dc::ServiceDiscovery();
        // conf.stringlist.push_back("Type=Basis");
        serviceDiscovery->addedComponentConnect(sigc::mem_fun(*this, 
            &RootModule::serviceAdded));
        serviceDiscovery->removedComponentConnect(sigc::mem_fun(*this, 
            &RootModule::serviceRemoved));

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
void RootModule::serviceAdded_(dfki::communication::ServiceEvent se)
{
    std::string envID;
    std::string type;
    std::string name;
    ModuleID::splitID(se.getServiceDescription().getName(), &envID, &type, &name);
            log(RTT::Info) << "connecting" << RTT::endlog();
    // Connect to the first appropriate MTA (same environment id).  
    if(mts == NULL && (type == "MTA" && envID == this->envID))
    {
        mts = connectToModule(se);
        if(mts != NULL)
        {
            globalLog(RTT::Info, "Connected to a message transport service (MTS)");
        }
    }

    // Build up a list with all the logging module IDs.  
    if(type == "LOG")
    {
        loggerSet.insert(se.getServiceDescription().getName());
    }
}

void RootModule::serviceRemoved_(dfki::communication::ServiceEvent se)
{
    std::string envID;
    std::string type;
    std::string name;
    ModuleID::splitID(se.getServiceDescription().getName(), &envID, &type, &name);

    // If its the MTA of this module, remove shortcut.
    if(type == "MTA" && envID == this->envID)
    {
        mts = NULL;
        globalLog(RTT::Warning, "My MTA has been removed.");
    } 

    // If its a logging module, remove entry in the logger list.
    if(type == "LOG")
    {
        loggerSet.erase(se.getServiceDescription().getName());
    }

    // Disconnect (will remove RemoteConnection object).
    disconnectFromModule(se);
}

////////////////////////////////METHODS////////////////////////////
bool RootModule::rpcConnectToModule(std::string const & remote_name, 
        std::string const & remote_ior)
{
    sem_wait(semaphoreConnect);
    // Create ports and connect local output to remote input.
    // Do nothing if the connection have already be established.
    if(remoteConnectionsMap.find(remote_name) != remoteConnectionsMap.end())
    {
        globalLog(RTT::Warning, "Connection to '%s' already established", 
            remote_name.c_str());
        sem_post(semaphoreConnect);
        return false;
    }

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
    /*
    if(!rm->getOutputPort()->createBufferConnection(*remoteinputport, 
            size_of_buffer, 
            RTT::ConnPolicy::LOCK_FREE))
    {
    */
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

    globalLog(RTT::Info, "Connected to '%s'", remote_name.c_str());
    remoteConnectionsMap.insert(std::pair<std::string, RemoteConnection*>(remote_name, rm));
    sem_post(semaphoreConnect);
    return true;
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
void RootModule::serviceAdded(dfki::communication::ServiceEvent se)
{
    serviceAdded_(se);
}

void RootModule::serviceRemoved(dfki::communication::ServiceEvent se)
{
    serviceRemoved_(se);
}
} // namespace modules

