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
 * \date    23.07.2010
 *
 * \version 0.3
 *          Integration of the new message generation and connection class.
 *
 * \version 0.2
 *          Added the message parser and generator and the corresponding functions.
 *          Renamed function for a better understanding.
 *          Changing visibility of some methods.
 *          Combining sendMessage-methods.
 *          Building up a logger name list instead of a connection-list.
 *          Cleaned connect and disconnect, functionality moved to serviceAdd/Removed.
 *          
 * \version 0.1 
 *          Added global logger method.
 *
 * \author  Stefan.Haase@dfki.de, Stanislav.Gutev@dfki.de
 */

#ifndef MODULES_ROOTMODULE_TASK_HPP
#define MODULES_ROOTMODULE_TASK_HPP

#include "root/ModuleBase.hpp"

#include <semaphore.h>
#include <stdint.h>
#include <stdarg.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include <boost/utility.hpp>

#include <service-discovery/ServiceDiscovery.h>

#include "module_id.h"
#include "messages/fipa_message.h"
#include "connections/corba_connection.h"

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

namespace orogen_transports {
    class TypelibMarshallerBase;
}

namespace root
{
class ConnectionInterface;

/**
 * Basis Module.
 * Supports basic function for message generation and communication.
 * Connects its MTA automatically and allows to create the connection
 * to other modules dynamically.
 */
class Module : public ModuleBase, boost::noncopyable
{
friend class ModuleBase;
 public:
    /**
     * Initializes parameters.
     * See configureHook() for further configurations.
     * \param name Name of the module. Will be set to \a _module_name in configureHook().
     */
    Module(std::string const& name = "root::Module");
    ~Module();

    ////////////////////////////////HOOKS////////////////////////////////
    /** This hook is called by Orocos when the state machine transitions
     * from Stopped to PreOperational, requiring the call to configureHook()
     * before calling start() again.
     */
    void cleanupHook(){};

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

    /** This hook is called by Orocos when the component is in the
     * RunTimeError state, at each activity step. See the discussion in
     * updateHook() about triggering options.
     *
     * Call recovered() to go back in the Runtime state.
     */
    void errorHook();

    /** This hook is called by Orocos when the state machine transitions
     * from Stopped to Running. If it returns false, then the component will
     * stay in Stopped. Otherwise, it goes into Running and updateHook()
     * will be called.
     */
    bool startHook();

    /** This hook is called by Orocos when the state machine transitions
     * from Running to Stopped after stop() has been called.
     */
    void stopHook();

    /** 
     * This hook is called by Orocos when the component is in the Running
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
     * Reads data and passes it to 'processMessage()'. Overwrite if you need 
     * another behaviour.
     */
    void updateHook();

 protected:
    /**
     * Converts the arguments into a string and calls the method 
     * globalLog(RTT::LoggerLevel log_type, std::string message) to send 
     * the message
     * \param log-type RTT::Never, RTT::Fatal, RTT::Critical, RTT::Error, 
     * RTT::Warning, RTT::Info, RTT::Debug, RTT::RealTime
     * \WARNING Safe against buffer overflows?
     */
    virtual void globalLog(RTT::LoggerLevel log_type, const char* format, ...);

    /**
     * The message, which is read within the updateHook(), is passed here.
     * This function can be overwritten to process the incoming data.
     * Without being overwritten, the module will send the message back to
     * the sender.
     */
    virtual bool processMessage(std::string& message);

    ////////////////////////////////RPC-METHODS//////////////////////////
    /**
     * RPC-method, used within 'connectToModule()' to create the ports on the
     * remote module and to connect the remote output to the local input.
     */
	bool rpcCreateConnectPorts(std::string const & remote_name, 
            std::string const & remote_ior);

    ////////////////////////////////CALLBACKS////////////////////////////
    /**
     * Callback function adds the newly discovered service if its unknown.
     * Default behaviour: Connects to the first appropriate (same environment) MTA.
     */
	virtual void serviceAdded_(std::string& remote_id, std::string& remote_ior);

    /**
     * Callback function removes the service from the list if it disappears.
     */
	virtual void serviceRemoved_(std::string& remote_id, std::string& remote_ior);

    ////////////////////////////////PARAMETER///////////////////////////
    FipaMessage fipa; /// Fipa message generator.
    std::map<std::string, ConnectionInterface*> connections; // Contains all connections.
    /**
     * Direct pointer to the connected Message Transport Service.
     * Use 'mta != NULL' to check whether fipa messages could be sent.
     */
    ConnectionInterface* mta;
    std::set<std::string> loggerNames; // All active logger modules.
    /**
     * Used to publish and collect services (modules).
     */
    rock::communication::ServiceDiscovery* serviceDiscovery;
    orogen_transports::TypelibMarshallerBase* transport;
    ModuleID modID; /// Contains the environment ID, the type and the name of the module.
    sem_t* connectSem; /// Prevents a simultaneous connection between two or more modules.

 private:
    /**
     * Use Module(std::string const& name) instead.
     */
    Module();
    ////////////////////////////////CALLBACKS////////////////////////////
    /**
     * Callback function adds the newly discovered service if its unknown.
     * Must not be overwritten, use serviceAdded_() instead.
     */
	void serviceAdded(rock::communication::ServiceEvent se);

    /**
     * Callback function removes the service from the list if it disappears.
     * Must not be overwritten, use serviceRemoved_() instead.
     */
	void serviceRemoved(rock::communication::ServiceEvent se);
};
} // namespace modules

#endif

