/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "EchoTask.hpp"
#include <fipa_acl/fipa_acl.h>

using namespace fipa_services;

EchoTask::EchoTask(std::string const& name)
    : EchoTaskBase(name)
{
}

EchoTask::EchoTask(std::string const& name, RTT::ExecutionEngine* engine)
    : EchoTaskBase(name, engine)
{
}

EchoTask::~EchoTask()
{
}



/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See EchoTask.hpp for more detailed
// documentation about them.

bool EchoTask::configureHook()
{
    if (! EchoTaskBase::configureHook())
        return false;

    mAgentName = _agent_name.get();
    return true;
}
bool EchoTask::startHook()
{
    if (! EchoTaskBase::startHook())
        return false;
    return true;
}
void EchoTask::updateHook()
{
    EchoTaskBase::updateHook();

    fipa::SerializedLetter serializedLetter;
    while(_letters.read(serializedLetter) == RTT::NewData)
    {
        fipa::acl::Letter letter = serializedLetter.deserialize();
        fipa::acl::ACLMessage msg = letter.getACLMessage();

        if(msg.getProtocol() == "ECHO" && msg.getPerformativeAsEnum() == fipa::acl::ACLMessage::REQUEST)
        {
            fipa::acl::ACLMessage responseMsg = msg;
                responseMsg.clearReceivers();

            fipa::acl::AgentIDList replyTo = msg.getAllReplyTo();
            if(replyTo.empty())
            {
                RTT::log(RTT::Warning) << "EchoTask: no reply to set -- sending answer to " << responseMsg.getSender().getName() << RTT::endlog();
                responseMsg.addReceiver( responseMsg.getSender() );
            } else {
                fipa::acl::AgentIDList::const_iterator cit = replyTo.begin();
                for(; cit != replyTo.end(); ++cit)
                {
                    RTT::log(RTT::Info) << "EchoTask: echo to receiver: '" << cit->getName() << RTT::endlog();
                    responseMsg.addReceiver(*cit);
                }
            }

            RTT::log(RTT::Info) << "EchoTask: from echo agent: '" << mAgentName << RTT::endlog();
            responseMsg.setSender( fipa::acl::AgentID(mAgentName) );
            responseMsg.setPerformative("inform");

            fipa::acl::ACLEnvelope responseLetter(responseMsg, letter.flattened().getACLRepresentation());
            fipa::SerializedLetter serializedResponseLetter(responseLetter, serializedLetter.representation);

            serializedResponseLetter.timestamp = base::Time::now();
            _handled_letters.write(serializedResponseLetter);
        } else {
            RTT::log(RTT::Warning) << "EchoTask: received letter, but no wrong protocol: " << msg.getProtocol()
                << "or not a request '" << msg.getPerformative() << "'" << RTT::endlog();
        }
    }
}
void EchoTask::errorHook()
{
    EchoTaskBase::errorHook();
}
void EchoTask::stopHook()
{
    EchoTaskBase::stopHook();
}
void EchoTask::cleanupHook()
{
    EchoTaskBase::cleanupHook();
}
