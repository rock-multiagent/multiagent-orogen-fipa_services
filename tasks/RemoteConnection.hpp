#ifndef MODULES_ROOTMODULE_REMOTECONNECTION_HPP
#define MODULES_ROOTMODULE_REMOTECONNECTION_HPP

#include <stdint.h>
#include <map>
#include <string>

#include <rtt/Ports.hpp>
#include <rtt/Logger.hpp>

#include <service-discovery/OrocosComponentService.h>

#include "root_module_data.h"

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)


/* Forward Declaration. */
namespace RTT{
    class NonPeriodicActivity;
    class TaskContext;
namespace Corba {
    class ControlTaskProxy;
}
}

namespace modules
{

/**
 * Contains the input/output ports to a remote module.
 * Copy and assign is not allowed -> use 'new IOPorts' in
 * combination with maps/vectors etc.
 */
class RemoteConnection
{  
 public:
    RemoteConnection(RTT::TaskContext* task_context_local, 
            dfki::communication::OrocosComponentRemoteService rms);

    RemoteConnection(RTT::TaskContext* task_context_local, 
        std::string remote_name, std::string remote_ior);

    ~RemoteConnection();

    inline std::string getRemoteModuleName() const
    {
        return remoteModuleName;
    }

    inline RTT::Corba::ControlTaskProxy* getControlTaskProxy() const
    {
        return controlTaskProxy;
    }

    inline std::string getLocalIOR() const
    {
        return localIOR;
    }

    inline std::string getLocalModuleName() const
    {
        return localModuleName;
    }

    inline RTT::InputPort<Vector>* getInputPort() const
    {
        return inputPort;
    }

    inline RTT::OutputPort<Vector>* getOutputPort() const
    {
        return outputPort;
    }

    inline std::string getOutputPortName() const
    {
        return outputPortName;
    }

    bool initialize();

    /**
     * Synchronize the Control Task Proxy.
     * Has to be done, if new ports have been added.
     * At the moment, the Control Task Proxy is deleted and reallocated.
     */
    bool syncControlTaskProxy();

 private:
    RemoteConnection();
    DISALLOW_COPY_AND_ASSIGN(RemoteConnection);

 private:
    std::string remoteModuleName;
    std::string remoteIOR;
    std::string localModuleName;
    std::string localIOR;
    std::string inputPortName;
    std::string outputPortName;

    RTT::TaskContext* taskContext;
    RTT::Corba::ControlTaskProxy* controlTaskProxy;
    RTT::InputPort<Vector>* inputPort;
    RTT::OutputPort<Vector>* outputPort; 
    bool isInitialized;

    //TODO: Add enum to which kind of module this
    // connection leads (MTS, BASIS, AKKU...)
};
} // namespace modules
#endif

