#ifndef MODULES_ROOTMODULE_TASK_HPP
#define MODULES_ROOTMODULE_TASK_HPP

#include "modules/RootModuleBase.hpp"
#include <stdint.h>

#include <list>
#include <map>
#include <string>

#include <rtt/Ports.hpp>

#include <service-discovery/OrocosComponentService.h>
#include <service-discovery/ServiceDiscovery.h>

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

/* Forward Declaration. */
namespace RTT
{
class NonPeriodicActivity;
} // namespace RTT

namespace modules
{
class RemoteConnection;

/**
 * Basis Modul.
 * Connect to the MTS automatically and allows to create the connection
 * to other modules dynamically (if neccessary).
 */
class RootModule : public RootModuleBase
{
friend class RootModuleBase;
 public:
    RootModule(std::string const& name = "modules::RootModule");
    ~RootModule();

    /**
     * Connects to the TaskContext 'rms'. The needed ports will be created
     * (on this and the remote module) and the output-ports will be connected 
     * with the input ports. 
     * @returns NULL if fails. 
     */
    RemoteConnection* connectToRemoteModule(dfki::communication::OrocosComponentRemoteService rms);

    void disconnectFromService(dfki::communication::OrocosComponentRemoteService rms);

    RTT::NonPeriodicActivity* getNonPeriodicActivity();

    /**
     * Checks whether a service is a MTS. 
     * TODO: Extend to requestType (MTS, Camera...).
     */
    bool isMTS(dfki::communication::OrocosComponentRemoteService rms);

    /**
     * Sends a Vector message to the connected MTS, which forwards it to the 
     * receiver. First, this service has to be connected to a MTS 
     * and the receiver must be known by the MTS.
     */
    bool sendMessage(std::string const& receiver, Vector const& msg);


    /**
     * Sends a string message to the connected MTS, which forwards it to the 
     * receiver. First, this service has to be connected to a MTS 
     * and the receiver must be known by the MTS.
     */
    bool sendMessage(std::string const& receiver, std::string const& msg);

    /**
     * The class 'ServiceDiscovery' creates the avahi client and is used to publish 
     * this module on the network (with 'OrocosComponentLocalService'), collect all 
     * other available services (with 'afServiceBrowser') and defines callbacks.
     */
    void startServiceDiscovery();

 public: // HOOKS
    /** This hook is called by Orocos when the state machine transitions
     * from PreOperational to Stopped. If it returns false, then the
     * component will stay in PreOperational. Otherwise, it goes into
     * Stopped.
     *
     * It is meaningful only if the #needs_configuration has been specified
     * in the task context definition with (for example):
     *
     *   task_context "TaskName" do
     *     needs_configuration
     *     ...
     *   end
     */
    bool configureHook();

    /** This hook is called by Orocos when the state machine transitions
     * from Stopped to Running. If it returns false, then the component will
     * stay in Stopped. Otherwise, it goes into Running and updateHook()
     * will be called.
     */
    bool startHook();

    /** This hook is called by Orocos when the component is in the Running
     * state, at each activity step. Here, the activity gives the "ticks"
     * when the hook should be called. See README.txt for different
     * triggering options.
     *
     * The warning(), error() and fatal() calls, when called in this hook,
     * allow to get into the associated RunTimeWarning, RunTimeError and
     * FatalError states. 
     *
     * In the first case, updateHook() is still called, and recovered()
     * allows you to go back into the Running state.  In the second case,
     * the errorHook() will be called instead of updateHook() and in the
     * third case the component is stopped and resetError() needs to be
     * called before starting it again.
     *
     */
    void updateHook();
    

    /** This hook is called by Orocos when the component is in the
     * RunTimeError state, at each activity step. See the discussion in
     * updateHook() about triggering options.
     *
     * Call recovered() to go back in the Runtime state.
     */
    // void errorHook();

    /** This hook is called by Orocos when the state machine transitions
     * from Running to Stopped after stop() has been called.
     */
    void stopHook();

    /** This hook is called by Orocos when the state machine transitions
     * from Stopped to PreOperational, requiring the call to configureHook()
     * before calling start() again.
     */
    void cleanupHook();

 protected:
    /**
     * Fills the service-discovery configuration struct with the module informations.
     * Extract the informations from 'configuration/module.xml' using the 'loadProperties()'
     * of Orocos. If the file or the properties can not be loaded, default values will
     * be used
     * Additionally it resets the name of the TaskContext!
     * If the file does not exists, it will be created.
     */
    void fillModuleInfo(std::string const & configuration);

 protected: // CALLBACKS
    /**
     * Callback function adds the newly discovered service if its unknown.
     */
	void serviceAdded(dfki::communication::OrocosComponentRemoteService rms);

    /**
     * Removes the service from the list if it disappears.
     */
	void serviceRemoved(dfki::communication::OrocosComponentRemoteService rms);

 protected: // METHODS
    /**
     * Used within 'connectToRemoteModule()' to create the ports on the
     * remote module and to connect the remote output to the local input.
     */
	bool createAndConnectPorts(std::string const & remote_name, 
            std::string const & remote_ior);

    /** Contains the service(module) informations, used to configure 'ServiceDiscovery'. */
    	dfki::communication::ServiceDiscovery::Configuration conf;

    /**
     * Contains all the connections (IO-ports and Control TaskProxy) 
     * to the other modules.
     */
        std::map<std::string, RemoteConnection*> remoteConnectionsMap;
    /**
     * Direct pointer to the connected Message Transport Service.
     * Use 'mts != NULL' to check whether fipa messages could be sent.
     */
       RemoteConnection* mts;
   

 private:
     /**
     * ServiceDiscovery is used to publish its service, collect all available services 
     * and define callbacks (service removed and added).
     */
    dfki::communication::ServiceDiscovery* serviceDiscovery;
    sem_t connectSemaphore; // TODO: Add semaphores in 'createAndConnectPorts()'.
    DISALLOW_COPY_AND_ASSIGN(RootModule);
};
} // namespace modules

#endif

