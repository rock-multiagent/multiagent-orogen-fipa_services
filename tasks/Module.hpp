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

#include <rtt/Ports.hpp>
#include <service-discovery/ServiceDiscovery.h>

#include "module_id.h"
#include "messages/fipa_message.h"

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
class Module : public ModuleBase
{
friend class ModuleBase;
 public:
    /**
     * Fills the service-discovery configuration struct with the module informations.
     * Extract the informations from 'configuration/module.xml' using the 'loadProperties()'
     * of Orocos. If the file or the properties can not be loaded, default values will
     * be used
     * Additionally it resets the module-ID of the TaskContext and splits and stores the ID
     * into envID, avahiType and nameAppendix. 
     * Uses \a configuration_file describing the path of the xml-conf-file.
     */
    Module(std::string const& name = "modules::RootModule",
            std::string const& conf_file = "");
    ~Module();

    /**
     * Connects to the TaskContext 'rms'. The needed ports will be created
     * (on this and the remote module) and the output-ports will be connected 
     * with the input ports. 
     * @returns NULL if fails. 
     */
    //RemoteConnection* connectToModule(dfki::communication::ServiceEvent se);

    /**
     * Deletes the connection to the module (ports and proxy).
     * If its the MTA of this module or a logging module, 
     * the shortcut is removed as well.
     */
    //void disconnectFromModule(dfki::communication::ServiceEvent se);

    inline RTT::NonPeriodicActivity* getNonPeriodicActivity()
    {
        return dynamic_cast< RTT::NonPeriodicActivity* >(getActivity().get());
    }

    /**
     * Returns the desired update frequency. Can be set within the 
     * configuration file with the double 'periodic_activity' field. Default is 0.01.
     * Is used by the child modules to set their update frequency.
     * The update frequency of this root-module has to be set using the orocos-file.
     */
    inline double getPeriodicActivity() const
    {
        return periodicActivity;
    }
    /**
     * Sends a fipa::BitefficientMessage message to the connected MTA, which forwards it to the 
     * receiver. First, this service has to be connected to a MTA 
     * and the receiver must be known by the MTA.
     */
//    bool sendMessage(std::string const& receiver,  boost::shared_ptr<fipa::BitefficientMessage> msg);
  //  bool sendMessage(std::string const& receiver,  fipa::BitefficientMessage msg);

    /**
     * Sends a string message to the connected MTA, which forwards it to the 
     * receiver. First, this service has to be connected to a MTA 
     * and the receiver must be known by the MTA.
     */
   // bool sendMessage(std::string const& receiver, std::string const& msg);

    /**
     * Sends the passed fipa message if a MTA is available.
     */
//    bool sendMessageToMTA(boost::shared_ptr<fipa::BitefficientMessage>);
  //  bool sendMessageToMTA(fipa::BitefficientMessage);        

 public: // HOOKS
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
    void stopHook(){};

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
     * Reads data and passes it to 'processMessage()'. Overwrite if you need 
     * another behaviour.
     */
    void updateHook(std::vector<RTT::PortInterface*> const& updated_ports);

 protected:
    /**
     * Sets the logger level.
     */
    void configureModule(std::string const& file);

    /**
     * Generates a FIPA message with the passed content and receivers.
     * Sender will be this module. 
     */
//    boost::shared_ptr<fipa::BitefficientMessage> generateMessage(const std::string& content, 
//            const std::set<std::string>& receivers);
//    fipa::BitefficientMessage generateMessage(const std::string& content, 
//            const std::set<std::string>& receivers);


    /**
     * Generates a FIPA message with the passed content and receivers.
     * Static because it is also needed within static member functions.
     */
//    static boost::shared_ptr<fipa::BitefficientMessage> generateMessage(const std::string& content, 
//            const std::string sender,
//            const std::set<std::string>& receivers);
//    static fipa::BitefficientMessage generateMessage(const std::string& content, 
//            const std::string sender,
//            const std::set<std::string>& receivers);

//    static fipa::BitefficientMessage generateMessage(const std::string& content, 
//        const std::string sender,
//        const std::set<std::string>& receivers,
//        const std::string& conversation_id);
    /**
     * TODO document
     */
    inline size_t getPayloadSize()
    {
        return 1;
    }

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
     */
    virtual bool processMessage(std::string& message){return true;};

    /**
     * This method will be overwritten in the logger module.
     */
    virtual bool report(RTT::InputPortInterface*){return true;};

    /**
     * The class 'ServiceDiscovery' creates the avahi client and is used to publish 
     * this module on the network (with 'ServiceEvent'), collect all 
     * other available services (with 'afServiceBrowser') and defines callbacks.
     */
    void startServiceDiscovery();

 protected: // CALLBACKS
    /**
     * Callback function adds the newly discovered service if its unknown.
     */
	virtual void serviceAdded_(dfki::communication::ServiceEvent& se);

    /**
     * Removes the service from the list if it disappears.
     */
	virtual void serviceRemoved_(dfki::communication::ServiceEvent& se);

 protected: // RPC-METHODS
    /**
     * Used within 'connectToModule()' to create the ports on the
     * remote module and to connect the remote output to the local input.
     */
	bool rpcCreateConnectPorts(std::string const & remote_name, 
            std::string const & remote_ior);

 protected:
    /** Fipa message generator. */
    FipaMessage fipa;

    /**
     * Contains all the connections.
     */
    std::map<std::string, ConnectionInterface*> connections;

    /** Contains the service(module) informations, used to configure 'ServiceDiscovery'. */
    dfki::communication::ServiceConfiguration conf;

    /** Contains the configuration file properties. */
    RTT::PropertyBag* propertyBag;

    /**
     * Contains the names of all active logger modules.
     */
    std::vector<std::string> loggerNames;

    /**
     * Direct pointer to the connected Message Transport Service.
     * Use 'mta != NULL' to check whether fipa messages could be sent.
     */
    //RemoteConnection* mta;
    ConnectionInterface* mta;
   
    /**
     * ServiceDiscovery is used to publish its service, collect all available services 
     * and define callbacks (service removed and added).
     */
    dfki::communication::ServiceDiscovery* serviceDiscovery;

    orogen_transports::TypelibMarshallerBase* transport;

	std::string configurationFilePath;

    ModuleID modID;
//    std::string envID; /// Contains the environmental ID of this module ID.
//    std::string type;  /// Contains the avahi type of this module.
//    std::string name;  /// Contains the name of this module. Empty for MTAs.
    double periodicActivity; /// Used for the update frequency of the child modules.

 private: // CALLBACKS (private because they can not be overwritten, use protected ones)
    /**
     * Callback function adds the newly discovered service if its unknown.
     */
	void serviceAdded(dfki::communication::ServiceEvent se);

    /**
     * Removes the service from the list if it disappears.
     */
	void serviceRemoved(dfki::communication::ServiceEvent se);

 private:
    sem_t* semaphoreConnect; // Used in createAndConnectPorts(). No needed because runs in own thread?
    DISALLOW_COPY_AND_ASSIGN(Module);
};
} // namespace modules

#endif

