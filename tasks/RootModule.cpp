#include "RootModule.hpp"

#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <rtt/corba/ControlTaskProxy.hpp>
#include <rtt/corba/ControlTaskServer.hpp>
#include <rtt/NonPeriodicActivity.hpp>

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
        remoteConnectionsMapLogger()
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
    // See 'cleanupHook()'.
}

void RootModule::globalLog(RTT::LoggerLevel log_type, const char* format, ...)
{
    int n = 100;	
	char buffer[512];
	va_list arguments;

	va_start(arguments, format);
	n = vsnprintf(buffer, sizeof(buffer), format, arguments);
	va_end(arguments);
    
    globalLog(log_type, std::string(buffer));
}

void RootModule::globalLog(RTT::LoggerLevel log_type, std::string message)
{
    // Global log, sending message to all log-modules.
    // Build logging string with 'RTT::Logger/modname/msg'.
    std::string msg = "x/" + this->getName() + "/" + message;
    msg[0] = static_cast<int>(log_type); // log_type 0-6

    // Send the message as a string to all logging modules using the direct ports.
    std::map<std::string, RemoteConnection*>::iterator it;
    for(it=remoteConnectionsMapLogger.begin(); 
            it != remoteConnectionsMapLogger.end(); it++)
    {
        RTT::OutputPort<Vector>* output_port = it->second->getOutputPort();
        if(output_port) 
        {
            struct Vector msg_vec(msg);
            output_port->write(msg_vec);
        }
    }
    // Local log.
    log(log_type) << message << RTT::endlog();
}

//todo: add the rm to the map at the end, delete rm if connection fails.
RemoteConnection* RootModule::connectToRemoteModule(dc::ServiceEvent se)
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
    // Use remote method to create needed ports on the remote module.
    RTT::Corba::ControlTaskProxy* ctp = rm->getControlTaskProxy();
    RTT::Method <bool(std::string const &, std::string const &)> create_remote_ports = 
            ctp->methods()->getMethod<bool(std::string const&, std::string const &)>("createAndConnectPorts");
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

    // buffer(LOCKED/LOCK_FREE, buffer size, keep last written value, true=pull(problem here) false=push)
    if(!rm->getOutputPort()->connectTo(*remoteinputport, RTT::ConnPolicy::buffer(20, RTT::ConnPolicy::LOCKED, false, false)))
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

void RootModule::disconnectFromService(dc::ServiceEvent se)
{    
    std::string mod_name = se.getServiceDescription().getName();
    map<std::string, RemoteConnection*>::iterator it;
    it=remoteConnectionsMap.find(mod_name);
    if(it != remoteConnectionsMap.end()) { // found
        // If its the MTS, remove shortcut to this service.
        std::string output_str = "Removed service '";
        if(it->second == mts)
        {
            mts = NULL;
            output_str = "Removed message transport service '";
        }
        // If its a logging module, remove entry in the log-mod-map.
        if(ModuleID::getType(mod_name) == "LOG")
        {
            remoteConnectionsMapLogger.erase(mod_name);
        }
        // Ports and control task proxy will be removed.
        delete(it->second); it->second = NULL;
        remoteConnectionsMap.erase(it);
        globalLog(RTT::Info, "%s %s '", output_str.c_str(), se.getServiceDescription().getName().c_str());
    } else {
        globalLog(RTT::Warning, "Connection to '%s' unknown, can not disconnect",
            se.getServiceDescription().getName().c_str());
    }
}

RTT::NonPeriodicActivity* RootModule::getNonPeriodicActivity()
{
    return dynamic_cast< RTT::NonPeriodicActivity* >(getActivity().get());
}

bool RootModule::sendMessage(std::string const& receiver, Vector const& msg)
{
    map<std::string, RemoteConnection*>::iterator it = remoteConnectionsMap.find(receiver);
    // Receiver known?
    if(it != remoteConnectionsMap.end())
    {
        // Output ports available?
        RTT::OutputPort<Vector>* output_port = it->second->getOutputPort();
        if(output_port) {
            // Convert message string to struct Vector (has to be done because '\0'
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
    map<std::string, RemoteConnection*>::iterator it = remoteConnectionsMap.find(receiver);
    // Receiver known?
    if(it != remoteConnectionsMap.end())
    {
        // Output ports available?
        RTT::OutputPort<Vector>* output_port = it->second->getOutputPort();
        if(output_port) {
            // Convert message string to struct Vector (has to be done because '\0'
            // within the string interupts sending)
            struct Vector msg_vec(msg);
            output_port->write(msg_vec);
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

bool RootModule::sendMessageToMTA(Vector const& msg)
{
    if(mts)
    {
        RTT::OutputPort<Vector>* output_port = mts->getOutputPort();
        if(output_port) 
        {
            output_port->write(msg);
            return true;
        }
    }
    return false;
}  

void RootModule::startServiceDiscovery()
{
    if(serviceDiscovery == NULL)
    {
        serviceDiscovery = new dc::ServiceDiscovery();
//        conf.stringlist.push_back("Type=Basis");
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

////////////////////////////////HOOKS///////////////////////////////
bool RootModule::configureHook()
{    
    if (RTT::log().getLogLevel() < RTT::Logger::Info)
    {
        RTT::log().setLogLevel( RTT::Logger::Info );
    }
    fillModuleInfo();
    startServiceDiscovery();
    return true;
}

bool RootModule::startHook()
{
    // start SD
    return true;
}

void RootModule::updateHook(std::vector<RTT::PortInterface*> const& updated_ports)
{
    // Check input ports.
    std::vector<RTT::PortInterface*>::const_iterator it;
    for(it = updated_ports.begin(); it != updated_ports.end(); ++it)
    { 
	    modules::Vector message;
	    RTT::InputPortInterface* read_port = dynamic_cast<RTT::InputPortInterface*>(*it);
	    ((RTT::InputPort<modules::Vector>*)read_port)->read(message);
	    globalLog(RTT::Info, "Received new message on port %s of size",
                (*it)->getName().c_str(), message.size());
	    delete read_port;
    }
	
    //globalLog(RTT::Info, "Ein Test");
/*    if(mts != NULL)
    {
        std::string test_str("Dies ist ein Test, es ist ein längerer String, damit ich auch sehe, ob ein Speicherverlust auftritt\
            oder ob alles funktioniert. Wenn alles geht, dann könnte es daran gelegen haben, dass das struct Vector keinen Destructor hatte\
            und daher den vector nicht gecleart hat.");
        char buffer[33];
        sprintf(buffer, "%d\n", test_counter);
        std::string test_nr(buffer);
        test_counter++;
        if(test_counter < 100000)
            sendMessage(mts->getRemoteModuleName(), test_nr + test_str);
    }
*/
}

void RootModule::stopHook()
{
    // stop SD?
}

void RootModule::cleanupHook()
{
}  

////////////////////////////////////////////////////////////////////
//                           PROTECTED                            //
////////////////////////////////////////////////////////////////////
void RootModule::fillModuleInfo()
{
    // Set default values. Random Name for the module.
    char buffer[sizeof(int)*8+1];
    srand(time(NULL));
    sprintf(buffer, "%d", rand() % 100000);

    conf.setName("A_ROOT_rimresmodule" + std::string(buffer));
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
        RTT::PropertyBag* pb = this->properties();
        // Returns NULL if the property could not be loaded. 
        RTT::Property<std::string>* p_name = pb->getProperty<std::string>("name");
        RTT::Property<std::string>* p_type = pb->getProperty<std::string>("avahi_type");
        RTT::Property<int>* p_port = pb->getProperty<int>("port");
        RTT::Property<int>* p_ttl = pb->getProperty<int>("ttl");

        if(p_name) conf.setName(p_name->get());
        if(p_type) conf.setType(p_type->get());
        if(p_port) conf.setPort(p_port->get());
        if(p_ttl)  conf.setTTL(p_ttl->get());
    }

    // Set name of this task context.
    this->setName(conf.getName());
}

////////////////////////////////CALLBACKS///////////////////////////
void RootModule::serviceAdded_(dfki::communication::ServiceEvent se)
{
    std::string envID;
    std::string type;
    std::string name;
    ModuleID::splitID(se.getServiceDescription().getName(), &envID, &type, &name);

    globalLog(RTT::Info, "type: %s", type.c_str());

    // Connect to the first appropriate MTS (same environment id).  
    if((ModuleID::getEnvID(this->getName()) == envID && type == "MTA") && mts == NULL)
    {
        mts = connectToRemoteModule(se);
        if(mts != NULL)
        {
            globalLog(RTT::Info, "Connected to a message transport service (MTS)");
            //sendMessage(mts->getRemoteModuleName(), "Hello MTS, i am " + this->getName());
        }
    }

    // TODO ?
    // Connect to every LOG-Module.  
    if(type == "LOG")
    {
        
        RemoteConnection* rem_con_log = connectToRemoteModule(se);
        if(rem_con_log != NULL)
        {
            // Creates a map with all logging modules connections.
            remoteConnectionsMapLogger.insert(std::pair<std::string, RemoteConnection*>
                (se.getServiceDescription().getName(), rem_con_log));
        }
    }
}

void RootModule::serviceRemoved_(dfki::communication::ServiceEvent se)
{
    disconnectFromService(se);
}

////////////////////////////////METHODS////////////////////////////
bool RootModule::createAndConnectPorts(std::string const & remote_name, 
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

