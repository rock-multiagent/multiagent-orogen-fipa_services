/*
 * \file    RootModule.hpp
 *  
 * \brief   Base class supporting module detection, configuration and dynamic port connection.
 *
 * \details Module will use ../configuration/module.xml for configuration and connect to
 *          the MTA with the same environment ID. Use \a connectToRemoteModule() to create
 *          a dynamic connection. All the connection are stored within the \a remoteConnectionsMap.
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    15.07.2010
 *
 * \version 0.1 
 *          Added global logger method.
 *
 * \author  Stefan.Haase@dfki.de, Stanislav.Gutev@dfki.de
 */

#ifndef MODULES_ROOTMODULE_TASK_HPP
#define MODULES_ROOTMODULE_TASK_HPP

#include "modules/RootModuleBase.hpp"

#include <semaphore.h>
#include <stdint.h>

#include <list>
#include <map>
#include <string>

#include <rtt/Ports.hpp>

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
 * Basis Module.
 * Connect to the MTS automatically and allows to create the connection
 * to other modules dynamically (if neccessary).
 */
class RootModule : public RootModuleBase
{
friend class RootModuleBase;
 public:
    RootModule(std::string const& name = "modules::RootModule",
            std::string const& conf_file = "");
    ~RootModule();

    /**
     * Sends the logging-message to all log-modules and store it
     * locally.
     * \param log-type RTT::Never, RTT::Fatal, RTT::Critical, RTT::Error, 
     * RTT::Warning, RTT::Info, RTT::Debug, RTT::RealTime
     */
    void globalLog(RTT::LoggerLevel log_type, std::string message);

    /**
     * Connects to the TaskContext 'rms'. The needed ports will be created
     * (on this and the remote module) and the output-ports will be connected 
     * with the input ports. 
     * @returns NULL if fails. 
     */
    RemoteConnection* connectToRemoteModule(dfki::communication::ServiceEvent se);

    /**
     * Deletes the connection to the module (ports and proxy).
     * If its the MTA of this module or a logging module, 
     * the shortcut is removed as well.
     */
    void disconnectFromService(dfki::communication::ServiceEvent se);

    RTT::NonPeriodicActivity* getNonPeriodicActivity();

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
     * Sends the passed fipa message if a MTA is available.
     */
    bool sendMessageToMTA(Vector const& msg);    

    /**
     * The class 'ServiceDiscovery' creates the avahi client and is used to publish 
     * this module on the network (with 'ServiceEvent'), collect all 
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
    void updateHook(std::vector<RTT::PortInterface*> const& updated_ports);
    

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
     * Uses \a configuration_file describing the path of the xml-conf-file.
     */
    void fillModuleInfo();

 protected: // CALLBACKS
    /**
     * Callback function adds the newly discovered service if its unknown.
     */
	void serviceAdded_(dfki::communication::ServiceEvent rms);

    /**
     * Removes the service from the list if it disappears.
     */
	void serviceRemoved_(dfki::communication::ServiceEvent rms);

 protected: // METHODS
    /**
     * Used within 'connectToRemoteModule()' to create the ports on the
     * remote module and to connect the remote output to the local input.
     */
	bool createAndConnectPorts(std::string const & remote_name, 
            std::string const & remote_ior);

 protected:
    /** Contains the service(module) informations, used to configure 'ServiceDiscovery'. */
    dfki::communication::ServiceConfiguration conf;

    /**
     * Contains all the connections (IO-ports and Control TaskProxy) 
     * to the other modules.
     */
    std::map<std::string, RemoteConnection*> remoteConnectionsMap;

    /**
     * Contains all the connections to the logger modules.
     */
    std::map<std::string, RemoteConnection*> remoteConnectionsMapLogger;

    /**
     * Direct pointer to the connected Message Transport Service.
     * Use 'mts != NULL' to check whether fipa messages could be sent.
     */
    RemoteConnection* mts;
   
    /**
     * ServiceDiscovery is used to publish its service, collect all available services 
     * and define callbacks (service removed and added).
     */
    dfki::communication::ServiceDiscovery* serviceDiscovery;

	std::string configuration_file;

 private: // CALLBACKS (private because they can not be overwritten, use protected ones)
    /**
     * Callback function adds the newly discovered service if its unknown.
     */
	void serviceAdded(dfki::communication::ServiceEvent rms);

    /**
     * Removes the service from the list if it disappears.
     */
	void serviceRemoved(dfki::communication::ServiceEvent rms);

 private:
    sem_t* semaphoreConnect; // Used in createAndConnectPorts().
    DISALLOW_COPY_AND_ASSIGN(RootModule);
};
} // namespace modules

#endif

