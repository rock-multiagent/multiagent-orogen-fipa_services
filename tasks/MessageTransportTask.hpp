/* Generated from orogen/lib/orogen/templates/tasks/Task.hpp */

#ifndef FIPA_SERVICES_MESSAGETRANSPORT_TASK_HPP
#define FIPA_SERVICES_MESSAGETRANSPORT_TASK_HPP

#include "fipa_services/MessageTransportTaskBase.hpp"

#include <map>
#include <vector>
#include <boost/thread.hpp>
#include <service_discovery/ServiceDiscovery.hpp>

namespace fipa {
namespace services {
    class Transport;
    
    namespace message_transport {
        class MessageTransport;
    }

} // namespace service
} // namespace fipa

namespace fipa_services {

    /*! \class MessageTransportTask
     * \brief The task context provides and requires services. It uses an ExecutionEngine to perform its functions.
     * Essential interfaces are operations, data flow ports and properties. These interfaces have been defined using the oroGen specification.
     * In order to modify the interfaces you should (re)use oroGen and rely on the associated workflow.
     *
     * This oroGen task represents a message transport task
     * to allow to allow for inter-robot communication in a p2p communication network.
     *
     * Typically this instance will be controlled from the ruby layer (refer to bindings)
     *
     * Calling the operation 'addReceiver' will trigger the creation of an output port of a predefined name, e.g. 'agent_0'
     *
     * The client can then connect to this output port and any message addressed to 
     * 'agent_0' will be delivered to this port.
     * This agent name
     *
     * \beginverbatim
         #include <fipa_acl/fipa_acl.h>

         fipa::acl::ACLMessage message;
         message.setPerformative(AclMessage::REQUEST);
         ...
         fipa::acl::Letter letter(message, representation::BITEFFICIENT);
         fipa::SerializedLetter serializedLetter(letter, representation::BITEFFICIENT);
     * \endverbatim
     *
     *
     * \details
     * The name of a TaskContext is primarily defined via:
     \verbatim
     deployment 'deployment_name'
         task('custom_task_name','fipa_services::MessageTransportTask')
     end
     \endverbatim
     *  It can be dynamically adapted when the deployment is called with a prefix argument. 
     */
    class MessageTransportTask : public MessageTransportTaskBase
    {
	friend class MessageTransportTaskBase;
    protected:
        mutable boost::mutex mConnectToMTSMutex; 
        mutable boost::shared_mutex mServiceChangeMutex; /// Prevents a simulatenous change of the service

        fipa::services::message_transport::MessageTransport* mMessageTransport;
        
        std::string mInterface;
        std::string mClientServiceType;
        
        // Service directory entries of other agents (where mDNS is not supported).
        // They will be all registered with the DSD.
        std::vector<fipa::services::ServiceDirectoryEntry> mExtraServiceDirectoryEntries;

        // Receiver ports for receivers that have been attached via the given operation
        typedef std::map<std::string, RTT::base::OutputPortInterface*> ReceiverPorts;
        ReceiverPorts mReceivers;

        /* Upon adding of a receiver, a new output port for this receiver is generated. Output port will be of receivers name (if successful)
         */
        virtual bool addReceiver(::std::string const & receiver, bool is_local = false);
        
        /* Retrieve list of currently attached receivers
         */
        virtual ::std::vector< ::std::string > getReceivers();

        /* Upon removal of a receiver, the corresponding output port is removed
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
        
        /**
         * Register a service (a receiver) with the distributed service directory.
         */
        void registerService(std::string receiver);
        /**
         * Deregister a service (a receiver) with the distributed service directory.
         */
        void deregisterService(std::string receiver);

        /**
         * Service added callback handler
         */
        void serviceAdded(servicediscovery::avahi::ServiceEvent event);

        /**
         * Service removed callback handler
         */
        void serviceRemoved(servicediscovery::avahi::ServiceEvent event);

        /**
         * Connect to another MTS using a known ior
         */
        void connectToMTS(const std::string& serviceName, const std::string& ior);
        
        /*
         * Local delivery to an output port
         */
        bool deliverLetterLocally(const std::string& receiverName, const fipa::acl::Letter& letter);

        /**
         * Initialize the message transport
         */
        void initializeMessageTransport();

    public:
        /** TaskContext constructor for MessageTransportTask
         * \param name Name of the task. This name needs to be unique to make it identifiable via nameservices.
         * \param initial_state The initial TaskState of the TaskContext. Default is Stopped state.
         */
        MessageTransportTask(std::string const& name = "fipa_services::MessageTransportTask");

        /** TaskContext constructor for MessageTransportTask
         * \param name Name of the task. This name needs to be unique to make it identifiable for nameservices. 
         * \param engine The RTT Execution engine to be used for this task, which serialises the execution of all commands, programs, state machines and incoming events for a task. 
         * 
         */
        MessageTransportTask(std::string const& name, RTT::ExecutionEngine* engine);

        /** Default deconstructor of MessageTransportTask
         */
	~MessageTransportTask();

        /** This hook is called by Orocos when the state machine transitions
         * from PreOperational to Stopped. If it returns false, then the
         * component will stay in PreOperational. Otherwise, it goes into
         * Stopped.
         *
         * It is meaningful only if the #needs_configuration has been specified
         * in the task context definition with (for example):
         \verbatim
         task_context "TaskName" do
           needs_configuration
           ...
         end
         \endverbatim
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
         * when the hook should be called.
         *
         * The error(), exception() and fatal() calls, when called in this hook,
         * allow to get into the associated RunTimeError, Exception and
         * FatalError states. 
         *
         * In the first case, updateHook() is still called, and recover() allows
         * you to go back into the Running state.  In the second case, the
         * errorHook() will be called instead of updateHook(). In Exception, the
         * component is stopped and recover() needs to be called before starting
         * it again. Finally, FatalError cannot be recovered.
         */
        void updateHook();

        /** This hook is called by Orocos when the component is in the
         * RunTimeError state, at each activity step. See the discussion in
         * updateHook() about triggering options.
         *
         * Call recover() to go back in the Runtime state.
         */
        void errorHook();

        /** This hook is called by Orocos when the state machine transitions
         * from Running to Stopped after stop() has been called.
         */
        void stopHook();

        /** This hook is called by Orocos when the state machine transitions
         * from Stopped to PreOperational, requiring the call to configureHook()
         * before calling start() again.
         */
        void cleanupHook();
    };
}

#endif

