/*
 * \file    Task.hpp
 *  
 * \brief   Base class supporting module detection, configuration and dynamic port connection.
 *
 * \details Task will use ../configuration/module.xml for configuration and connect to
 *          the MTA with the same environment ID. Use \a connectToRemoteTask() to create
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

#ifndef MTS_TASK_HPP
#define MTS_TASK_HPP

#include "mts/TaskBase.hpp"

#include <stdint.h>
#include <stdarg.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include <boost/utility.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <service_discovery/service_discovery.h>

#include "connections/corba_connection.h"

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)


namespace sd = servicediscovery;

namespace fipa {
namespace service {
namespace message_transport {
    class MessageTransport;
} // end message_transport
} // end services
} // end fipa

namespace mts
{
class ConnectionInterface;

/**
 * Supports basic function for message generation and communication.
 * Connects its MTA automatically and allows to create the connection
 * to other modules dynamically.
 */
class Task : public TaskBase, boost::noncopyable
{
friend class TaskBase;
 public:
    /**
     * Initializes parameters.
     * See configureHook() for further configurations.
     * \param name Name of the module. Will be set to \a _module_name in configureHook().
     */
    Task(std::string const& name = "mts::Task");
    Task(std::string const& name, RTT::ExecutionEngine* engine); 
    ~Task();

    ////////////////////////////////HOOKS////////////////////////////////
    /** This hook is called by Orocos when the state machine transitions
     * from Stopped to PreOperational, requiring the call to configureHook()
     * before calling start() again.
     */
    void cleanupHook();

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
    virtual bool configureHook();

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
    // void stopHook();

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

    /**
     * Update information about another mts
     * \return true if update succeeded, false otherwise
     */
    virtual bool updateAgentList(const std::string& otherMTS, const std::vector<std::string>& agentList);

 protected:
    /**
     * The message, which is read within the updateHook(), is passed here.
     * This function can be overwritten to process the incoming data.
     * Without being overwritten, the module will send the message back to
     * the sender.
     * \param message message content (e.g. bitefficient encoded fipa message)
     */
    bool deliverOrForwardLetter(const fipa::acl::Letter& letter);

    ////////////////////////////////RPC-METHODS//////////////////////////
    /* Handler for the adding a receiver operation
     */
    virtual bool addReceiver(::std::string const & receiver);

    /* Handler for the remove a receiver operation
     */
    virtual bool removeReceiver(::std::string const & receiver);

    /**
    * Add an output port for a specific receiver, portname and receivername
    * will be identical
    * \return true on success, false otherwise
    */
    bool addReceiverPort(RTT::base::OutputPortInterface* outputPort, const std::string& name);

    /**
     * Remove the output port for a specific receiver
     * \return true on success, false otherwise
     */
    bool removeReceiverPort(const std::string& name);

   
    // Receiver ports for receivers that have been attached via the given operation
    typedef std::map<std::string, RTT::base::OutputPortInterface*> ReceiverPorts;
    ReceiverPorts mReceivers;

    /**
     * Retrieve client list of this message transport service
     * \return current list of attached receivers
     */
    std::vector<std::string> getReceivers();


    bool isConnectedTo(const std::string& name) const;

    /**
     * Retrieve connection to Agent
     */
    ConnectionInterface* getConnectionToAgent(const std::string& name) const;

    /**
     * Triggers an agent list update on all connected mts
     */
    void triggerRemoteAgentListUpdate(const std::string& mts = "");

    ////////////////////////////////CALLBACKS////////////////////////////
    /**
     * Callback function adds the newly discovered service if its unknown.
     * Default behaviour: Connects to the first appropriate (same environment) MTA.
     * \param buffer_size Message buffer size for the connection which might be created
     */
	virtual void serviceAdded_(std::string& remote_id, std::string& remote_ior, uint32_t buffer_size = 100);

    /**
     * Callback function removes the service from the list if it disappears.
     */
	virtual void serviceRemoved_(std::string& remote_id, std::string& remote_ior);

    static std::string itostr(int num)
    {
        char buffer[128];
        int n = 0;
        n=std::sprintf (buffer, "%d", num);
        std::string str(buffer);
        return n>0 ? str : "";
    }

    ////////////////////////////////PARAMETER///////////////////////////
    /**
     * Used to publish and collect services (modules).
     */
    sd::ServiceDiscovery* mServiceDiscovery;

    std::map<std::string, ConnectionInterface*> mConnections; // Contains all connections.
    mutable boost::shared_mutex mConnectionsMutex; /// Prevents a simultaneous access to the list of modules

    std::map<std::string, ::base::Stats<double> > mPortStats;
    
    // Buffersize of the connection which will be created between dynamically added components
    uint32_t mConnectionBufferSize; 

    fipa::service::message_transport::MessageTransport* mMessageTransport;
    //fipa::service::service_directory::ServiceDirectory* mServiceDirectory;

 private:
    /**
     * Use Task(std::string const& name) instead.
     */
    Task();
    ////////////////////////////////CALLBACKS////////////////////////////
    /**
     * Callback function adds the newly discovered service if its unknown.
     * Must not be overwritten, use serviceAdded_() instead.
     */
	void serviceAdded(sd::ServiceEvent se);

    /**
     * Callback function removes the service from the list if it disappears.
     * Must not be overwritten, use serviceRemoved_() instead.
     */
	void serviceRemoved(sd::ServiceEvent se);
};
} // namespace modules

#endif

