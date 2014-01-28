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
    class ServiceDirectory;

namespace message_transport {
    class MessageTransport;
} // namespace message_transport
} // namespace service
} // namespace fipa

namespace fipa_services {

    /*! \class MessageTransportTask
     * \brief The task context provides and requires services. It uses an ExecutionEngine to perform its functions.
     * Essential interfaces are operations, data flow ports and properties. These interfaces have been defined using the oroGen specification.
     * In order to modify the interfaces you should (re)use oroGen and rely on the associated workflow.
     * The FIPA message bus relies on the application of so called MTS's
(Message Transport Services) - mainly to allow
for inter-robot communication.

One MTS per robot/environment is responsible for delivering message
to a set of components, here identified by an environment id as part
which is part of the components name: <env-id>_<type>.

An MTS will be recognized by its type suffix: <env-id>_MTA, where
the <env-id> follows <robot-type>_<id> (thus the second underscore
separates <env-id> and <type>, e.g. crex_0_MTA)

An MTS allows to register
receivers and will write the information to the receivers
according to the receivers named in the FIPA message

Warning: root modules and derived components should *NOT* be started with
the '--sd-domain' option. Instead the property 'avahi_type' should be used,
to set domain and type (_udp,_tcp).
Otherwise a runtime condition exists between receiving events from the
underlying service_discovery and registering callbacks for these events.
!! -- this requires to be fixed in upcoming versions -- !!
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
        fipa::services::ServiceDirectory* mGlobalServiceDirectory;
        fipa::services::ServiceDirectory* mLocalServiceDirectory;

        base::Time mLocalServiceDirectoryTimestamp;

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
         * The message, which is read within the updateHook(), is passed here.
         * This function can be overwritten to process the incoming data.
         * Without being overwritten, the module will send the message back to
         * the sender.
         * \param message message content (e.g. bitefficient encoded fipa message)
         */
        bool deliverOrForwardLetter(const fipa::acl::Letter& letter);

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

