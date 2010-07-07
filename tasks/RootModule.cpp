#include "RootModule.hpp"

#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <rtt/corba/ControlTaskProxy.hpp>
#include <rtt/corba/ControlTaskServer.hpp>
#include <rtt/NonPeriodicActivity.hpp>

#include "RemoteConnection.hpp"

namespace dc = dfki::communication;
namespace rc = RTT::Corba;

int test_counter = 0;

namespace modules
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
RootModule::RootModule(std::string const& name) : RootModuleBase(name),
        serviceDiscovery(NULL),
        mts(NULL)
{
    conf = dc::ServiceDiscovery::Configuration("", name, "_rimres._tcp");
    sem_init(&connectSemaphore, 1, 1); // Shared between processes and value one.
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
    // See 'cleanupHook()'.
}

//todo: add the rm to the map at the end, delete rm if connection fails.
RemoteConnection* RootModule::connectToRemoteModule(dc::OrocosComponentRemoteService rms)
{
    // Do nothing if the connection have already be established.
    if(remoteConnectionsMap.find(rms.getName()) != remoteConnectionsMap.end())
    {
        log(RTT::Warning) << "Connection to '" << rms.getName() << 
                "' already established. " << RTT::endlog();
        return NULL;
    }
    // Do not connect to yourself
    if(rms.getName() == conf.name)
    {
        log(RTT::Warning) << "Modules are not allowed to connect to themselves." << 
            RTT::endlog();
        return NULL;
    }
    // Create and add new connection.
    RemoteConnection* rm = new RemoteConnection(this, rms);
    if(!rm->initialize())
    {
        log(RTT::Error) << "Connection to '" << rms.getName() << 
            "' could not be initialized." << RTT::endlog(); 
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
        log(RTT::Error) << "Connection on the remote module '" << rms.getName() << 
            "' could not be created." << RTT::endlog();
        delete rm; rm = NULL;
        return NULL;
    }
    // Create remote connection.
    if(!create_remote_ports(rm->getLocalModuleName(), rm->getLocalIOR()))
    {
        log(RTT::Error) << "Connection on the remote module '" << rms.getName() << 
            "' could not be created." << RTT::endlog();
        delete rm; rm = NULL;
        return NULL;
    }

    // Refresh control task proxy.
    if(!rm->syncControlTaskProxy())
    {
        log(RTT::Error) << "Connection to '" << rms.getName() <<
            "' could not be reestablished." << RTT::endlog();
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
        log(RTT::Error) << "Inputport '" << rm->getOutputPortName() <<
            "' of the remote module '" << rms.getName() << 
            "' could not be received." << RTT::endlog();
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
        log(RTT::Error) << "Outputport '" << rm->getOutputPortName() <<
            "' cant be connected to the Inputport of " <<
            rms.getName() << RTT::endlog();
        delete rm; rm = NULL;
        return NULL;
    }
    log(RTT::Info) << "Connected to '" <<  rms.getName() << "'" << RTT::endlog();
    // Add to map of valid connections.
    remoteConnectionsMap.insert(std::pair<std::string, RemoteConnection*>
            (rms.getName(), rm));
    return rm;
} 

void RootModule::disconnectFromService(dc::OrocosComponentRemoteService rms)
{    
    map<std::string, RemoteConnection*>::iterator it;
    it=remoteConnectionsMap.find(rms.getName());
    if(it != remoteConnectionsMap.end()) { // found
        // If its the MTS, remove shortcut to this service.
        std::string output_str = "Removed service '";
        if(it->second == mts)
        {
            mts = NULL;
            output_str = "Removed message transport service '";
        }
        // Ports and control task proxy will be removed.
        delete(it->second); it->second = NULL;
        remoteConnectionsMap.erase(it);
        log(RTT::Info) << output_str << rms.getName() << "'" << RTT::endlog();
    } else {
        log(RTT::Warning) << "Connection to '" << rms.getName() << 
            "' unknown, can not disconnect." << RTT::endlog();
    }
}

RTT::NonPeriodicActivity* RootModule::getNonPeriodicActivity()
{
    return dynamic_cast< RTT::NonPeriodicActivity* >(getActivity().get());
}

bool RootModule::isMTS(dfki::communication::OrocosComponentRemoteService rms)
{
    return (rms.getName().find("transport") != std::string::npos);
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
            log(RTT::Warning) << "No output ports available for receiver " <<
                receiver << ", message could not be sent." << RTT::endlog();
            return false;
        }
    } else {
        log(RTT::Warning) << "Receiver " << receiver << 
                " unknown, message could not be sent." << RTT::endlog();
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
            output_port->write(msg);
            return true;
        } else {
            log(RTT::Warning) << "No output ports available for receiver " <<
                receiver << ", message could not be sent." << RTT::endlog();
            return false;
        }
    } else {
        log(RTT::Warning) << "Receiver " << receiver << 
                " unknown, message could not be sent." << RTT::endlog();
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
        conf.stringlist.push_back("Type=Basis");
        serviceDiscovery->addedComponentConnect(sigc::mem_fun(*this, 
            &RootModule::serviceAdded));
        serviceDiscovery->removedComponentConnect(sigc::mem_fun(*this, 
            &RootModule::serviceRemoved));
    }
    serviceDiscovery->configure(conf);
    try{
        serviceDiscovery->start();
    } catch(exception& e) {
        log(RTT::Error) << e.what() << RTT::endlog();
    }
    log(RTT::Info) << "Started service '" << this->getName() << "' with avahi-type '" 
        << conf.avahi_type << "'" << RTT::endlog();
}

////////////////////////////////HOOKS///////////////////////////////
bool RootModule::configureHook()
{    
    if (RTT::log().getLogLevel() < RTT::Logger::Info)
    {
        RTT::log().setLogLevel( RTT::Logger::Info );
    }
    fillModuleInfo("");
    startServiceDiscovery();
    return true;
}

bool RootModule::startHook()
{
    // start SD
    return true;
}

void RootModule::updateHook()
{
    // Check input ports.
    map<std::string, RemoteConnection*>::iterator it = remoteConnectionsMap.begin();
    for(; it != remoteConnectionsMap.end(); it++)
    {
        Vector vec;
        std::string msg;
        RTT::InputPort<Vector>* input = it->second->getInputPort();
        if(!input)
        {
            continue;
        }
        if(input->read(vec))
        {
            msg = vec.toString();
            log(RTT::Info) << it->first << ": " << msg << RTT::endlog();
            input->clear();
        }
    }
    if(mts != NULL)
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
void RootModule::fillModuleInfo(std::string const & configuration)
{
    // Set default values. Random Name for the module.
    char buffer[sizeof(int)*8+1];
    srand(time(NULL));
    sprintf(buffer, "%d", rand() % 100000);
    
    conf.name = "rimres_module_" + std::string(buffer);
    conf.avahi_type = "_rimres._tcp";
    conf.avahi_port = 12000;
    conf.ttl = 0;
    conf.IOR = RTT::Corba::ControlTaskServer::getIOR(this);

    // Try to load the config-file.
    std::string module_path = "";
    if (configuration != "")
	module_path = configuration;
    else 
	module_path = "../configuration/module.xml";
    if(!this->marshalling()->loadProperties(module_path))
    {
        log(RTT::Warning) << "Could not load properties " <<
            module_path << ", default values are used." << RTT::endlog();
    } else {
        RTT::PropertyBag* pb = this->properties();
        // Returns NULL if the property could not be loaded. 
        RTT::Property<std::string>* p_name = pb->getProperty<std::string>("name");
        RTT::Property<std::string>* p_type = pb->getProperty<std::string>("avahi_type");
        RTT::Property<int>* p_port = pb->getProperty<int>("port");
        RTT::Property<int>* p_ttl = pb->getProperty<int>("ttl");

        if(p_name) conf.name = p_name->get();
        if(p_type) conf.avahi_type = p_type->get();
        if(p_port) conf.avahi_port = p_port->get();
        if(p_ttl)  conf.ttl = p_ttl->get();
    }

    // Set name of this task context.
    this->setName(conf.name);
}

test()

////////////////////////////////CALLBACKS///////////////////////////
void RootModule::serviceAdded(dfki::communication::OrocosComponentRemoteService rms)
{
    // not looking particularly for a MTS (for testing purposes)
    // Connect to the first appropriate MTS.
    if(isMTS(rms) && mts == NULL)
    {
        mts = connectToRemoteModule(rms);
        if(mts != NULL)
        {
            log(RTT::Info) << "Connected to a message transport service (MTS)" << RTT::endlog();
            sendMessage(mts->getRemoteModuleName(), "Hello MTS, i am " + this->getName());
        }
    }
}

void RootModule::serviceRemoved(dfki::communication::OrocosComponentRemoteService rms)
{
    
    if(remoteConnectionsMap.find(rms.getName()) != remoteConnectionsMap.end())
    {
        disconnectFromService(rms);
    }
}

////////////////////////////////METHODS////////////////////////////
//add connection to the list at the end
bool RootModule::createAndConnectPorts(std::string const & remote_name, 
        std::string const & remote_ior)
{
    // Create ports and connect local output to remote input.
    // Do nothing if the connection have already be established.
    if(remoteConnectionsMap.find(remote_name) != remoteConnectionsMap.end())
    {
        log(RTT::Warning) << "Connection to '" << remote_name << 
                "' already established. " << RTT::endlog();
        return false;
    }

    // Create and add new connection.
    RemoteConnection* rm =  new RemoteConnection(this, remote_name, remote_ior);
    if(!rm->initialize())
    {
        log(RTT::Error) << "Connection to '" << remote_name << 
            "' could not be initialized." << RTT::endlog();
        delete rm; rm = NULL;
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
        log(RTT::Error) << "Inputport '" << rm->getOutputPortName() <<
            "' of the remote module '" << remote_name << 
            "' could not be received." << RTT::endlog();
        delete rm; rm = NULL;
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
        log(RTT::Error) << "Outputport '" << rm->getOutputPortName() <<
            "' cant be connected to the Inputport of '" <<
            remote_name << "'" << RTT::endlog();
        delete rm; rm = NULL;
        return false;
    }

    log(RTT::Info) << "Connected to '" <<  remote_name << "'" << RTT::endlog();
    remoteConnectionsMap.insert(std::pair<std::string, RemoteConnection*>(remote_name, rm));
    return true;
}

} // namespace modules

