#include "RemoteConnection.hpp"

#include <rtt/corba/ControlTaskProxy.hpp>
#include <rtt/corba/ControlTaskServer.hpp>

namespace modules
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
RemoteConnection::RemoteConnection(RTT::TaskContext* task_context_local, 
            dfki::communication::ServiceEvent se) :
            taskContext(task_context_local),
            controlTaskProxy(NULL),
            inputPort(NULL),
            outputPort(NULL)
{
    if (RTT::log().getLogLevel() < RTT::Logger::Info)
    {
        RTT::log().setLogLevel( RTT::Logger::Info );
    }

    remoteModuleName = se.getServiceDescription().getName();
    remoteIOR = se.getServiceDescription().getDescription("IOR");
    log(RTT::Info) << "remote IOR: " << remoteIOR << RTT::endlog();
    localModuleName = task_context_local->getName();
    localIOR = RTT::Corba::ControlTaskServer::getIOR(taskContext);    
    inputPortName = remoteModuleName + "->" + localModuleName + "_port";    
    outputPortName = localModuleName + "->" + remoteModuleName + "_port";
    isInitialized = false;

}

RemoteConnection::RemoteConnection(RTT::TaskContext* task_context_local, 
    std::string remote_name, std::string remote_ior) :
        taskContext(task_context_local),
        controlTaskProxy(NULL),
        inputPort(NULL),
        outputPort(NULL)
{
    remoteModuleName = remote_name;
    remoteIOR = remote_ior;
    localModuleName = task_context_local->getName();
    localIOR = RTT::Corba::ControlTaskServer::getIOR(taskContext);
    inputPortName = remoteModuleName + "->" + localModuleName + "_port";    
    outputPortName = localModuleName + "->" + remoteModuleName + "_port";
    isInitialized = false;

    if (RTT::log().getLogLevel() < RTT::Logger::Info)
    {
        RTT::log().setLogLevel( RTT::Logger::Info );
    }
}

RemoteConnection::~RemoteConnection()
{
    if(controlTaskProxy != NULL)
    {
        taskContext->removePeer(remoteIOR);
        delete controlTaskProxy;
        controlTaskProxy = NULL;
    }
    if(inputPort != NULL)
    {
        taskContext->ports()->removePort(inputPortName);
        delete inputPort; inputPort = NULL;
    }
    if(outputPort != NULL)
    {
        taskContext->ports()->removePort(outputPortName);
        delete outputPort; outputPort = NULL;
    }
}

bool RemoteConnection::initialize()
{
    if(isInitialized)
    {
        return true;
    }
    // Create Ports.
    inputPort = new RTT::InputPort<Vector>(inputPortName);
    outputPort = new RTT::OutputPort<Vector>(outputPortName);

    std::string info = "Input port from '" + remoteModuleName + "' to '" + localModuleName + "'";
    if(taskContext->ports()->addEventPort(inputPort, info)){
	log(RTT::Info) << "Create InputPort '" <<  inputPortName << "'" << RTT::endlog();
	if (taskContext->connectPortToEvent(inputPort)) {
	        log(RTT::Info) << "Port '" <<  inputPortName << "' connected to an event handler" << RTT::endlog();
	} else {
	        log(RTT::Error) << "Could not connect port '" <<  inputPortName << "' to an event handler." << RTT::endlog();
	}
    } else {
        log(RTT::Error) << "Create InputPort '" <<  inputPortName << "'" << RTT::endlog();
        return false;
    }
    info = "Output port from '" + localModuleName + "' to '" + remoteModuleName + "'";
    if(taskContext->ports()->addPort(outputPort, info)) {
        log(RTT::Info) << "Create OutputPort '" << outputPortName << "'" << RTT::endlog();
    } else {
        log(RTT::Error) << "Create OutputPort '" << outputPortName << "'" << RTT::endlog();
        return false;
    }

    // Create Control Task Proxy.
    RTT::Corba::ControlTaskProxy::InitOrb(0, 0);
    controlTaskProxy = RTT::Corba::ControlTaskProxy::
            Create(remoteIOR, remoteIOR.substr(0,3) == "IOR");
    // Creating a one-directional connection from task_context to the peer. 
    if(taskContext->addPeer(controlTaskProxy))
        log(RTT::Info) << "Create ControlTaskProxy to '" <<  
                remoteModuleName << "'" << RTT::endlog();
    else {
        log(RTT::Error) << "Can not create ControlTaskProxy to '" <<  
                remoteModuleName << "'" << RTT::endlog();
        return false;
    }
    
    isInitialized = true;
    return true;
}

bool RemoteConnection::syncControlTaskProxy()
{
    if(!isInitialized)
    {   
        return false;
    }

    if(controlTaskProxy != NULL)
    {
        taskContext->removePeer(remoteIOR);
        delete controlTaskProxy;
        controlTaskProxy = NULL;
    }
    // Create Control Task Proxy
    RTT::Corba::ControlTaskProxy::InitOrb(0, 0);
    controlTaskProxy = RTT::Corba::ControlTaskProxy::
            Create(remoteIOR, remoteIOR.substr(0,3) == "IOR");
    // Creating a one-directional connection from task_context to the peer. 
    if(taskContext->addPeer(controlTaskProxy))
        log(RTT::Info) << "Recreate ControlTaskProxy to '" <<  
                remoteModuleName << "'" << RTT::endlog();
    else {
         log(RTT::Error) << "Can not recreate ControlTaskProxy to '" <<  
                remoteModuleName << "'" << RTT::endlog();
        return false;
    }
    return true;
}
} // namespace modules
