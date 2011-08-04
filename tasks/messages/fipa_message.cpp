#include "fipa_message.h"

#include <iostream>

#include <fipa_acl/bitefficient_message.h>

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
std::string fipa_parameters[] = {"PERFORMATIVE", "SENDER", "RECEIVER", "REPLY-TO", "CONTENT", 
            "LANGUAGE", "ENCODING", "ONTOLOGY", "PROTOCOL", "CONVERSATION-ID", 
            "REPLY-WITH", "IN-REPLY-TO", "REPLY-BY"};
FipaMessage::FipaMessage() : 
    MessageInterface(fipa_parameters, 13), 
            aclMSG(NULL)
{
}

void FipaMessage::decode(std::string const& message)
{
    fa::MessageParser parser = fa::MessageParser();
    fa::ACLMessage aclmsg;
    if(!parser.parseData(message, aclmsg))
    {
        throw new MessageException("Fipa message could not be parsed.");
    }
    // Clears the parameter map.
    clear();

    std::map<std::string, MessageParameter>::iterator it;
    std::vector<fa::AgentID> agent_ids; 
    std::string entry;

    // Empty entries will be skipped.
    if(!(entry = aclmsg.getPerformative()).empty()) 
        parameters.find("PERFORMATIVE")->second.addEntry(entry);
    if(!(entry = aclmsg.getSender().getName()).empty()) 
        parameters.find("SENDER")->second.addEntry(entry);

    agent_ids = aclmsg.getAllReceivers();
    it = parameters.find("RECEIVER");
    for(unsigned int i=0; i<agent_ids.size(); i++)
        it->second.addEntry(agent_ids[i].getName());

    agent_ids = aclmsg.getAllReplyTo();
    it = parameters.find("REPLY-TO");
    for(unsigned int i=0; i<agent_ids.size(); i++)
        it->second.addEntry(agent_ids[i].getName());    

    if(!(entry = aclmsg.getContent()).empty()) 
        parameters.find("CONTENT")->second.addEntry(entry);
    /*
    it = parameters.find("CONTENT");
    std::string content = aclmsg.getContent();
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(" ");
    tokenizer tokens(content, sep);
    for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
    {
        it->second.addEntry(*tok_iter);
    }
    */

    if(!(entry = aclmsg.getLanguage()).empty()) 
        parameters.find("LANGUAGE")->second.addEntry(entry);
    if(!(entry = aclmsg.getEncoding()).empty()) 
        parameters.find("ENCODING")->second.addEntry(entry);
    if(!(entry = aclmsg.getOntology()).empty()) 
        parameters.find("ONTOLOGY")->second.addEntry(entry);
    if(!(entry = aclmsg.getProtocol()).empty()) 
        parameters.find("PROTOCOL")->second.addEntry(entry);
    if(!(entry = aclmsg.getConversationID()).empty()) 
        parameters.find("CONVERSATION-ID")->second.addEntry(entry);
    if(!(entry = aclmsg.getReplyWith()).empty()) 
        parameters.find("REPLY-WITH")->second.addEntry(entry);
    if(!(entry = aclmsg.getInReplyTo()).empty()) 
        parameters.find("IN-REPLY-TO")->second.addEntry(entry);
    if(!(entry = aclmsg.getReplyBy1()).empty()) 
        parameters.find("REPLY-BY")->second.addEntry(entry);
}

std::string FipaMessage::encode()
{
    if(!createACLMessage())
        throw MessageException("ACL-Message could not be created.");
    // Create bit-efficient byte-string.
    fa::ACLMessageOutputParser generator = fa::ACLMessageOutputParser();
    generator.setMessage(*aclMSG);
    try{
        std::string bitmsg = generator.getBitMessage(); 
        return bitmsg;
    } catch(MessageGeneratorException& e) {
        throw MessageException("Bit encoding failed: " + std::string(e.what()));
    }
}

bool FipaMessage::setParameter(std::string const& parameter, 
        std::vector<std::string> const& entries)
{
    std::map<std::string, MessageParameter>::iterator it;
    it = parameters.find(parameter);
    if(it != parameters.end()) {
        it->second.entries = entries;
        return true;
    } else {
        return false;
    }
}

bool FipaMessage::setParameter(std::string const& parameter,
        std::set<std::string> const& entries)
{
    std::vector<std::string> entries_vector;
    std::set<std::string>::iterator it_input = entries.begin();
    for(it_input=entries.begin(); it_input != entries.end(); it_input++)
    {
        entries_vector.push_back(*it_input);
    }
    return setParameter(parameter, entries_vector);
}

////////////////////////////////////////////////////////////////////
//                           PRIVATE                              //
////////////////////////////////////////////////////////////////////
bool FipaMessage::createACLMessage()
{
    // Create a new ACLMessage object.
    if(aclMSG != NULL)
    {
        delete aclMSG;
        aclMSG = NULL;
    }
    if(getEntry("PERFORMATIVE").size() == 0)
        aclMSG = new fa::ACLMessage(fa::ACLMessage::INFORM);
    else
        aclMSG = new fa::ACLMessage();

    // Runs through the parameters and fill the ACLMessage.
    std::map<std::string, MessageParameter>::iterator it;
    for(it=parameters.begin(); it != parameters.end(); ++it)
    {
        // LOOKUP MAP von string name auf funktionspointer?

        std::string param = it->first;
        // Continue if parameter is not used.
        if(it->second.getSize() == 0)
            continue;

        if(param == "PERFORMATIVE") {
            aclMSG->setPerformative(it->second.entries[0]);
//                std::cerr << "Performatives could not be set." << std::endl;
  //              return false;
 //           }
        } else if(param == "SENDER") {
            fa::AgentID sender = fa::AgentID(it->second.entries[0]);
            aclMSG->setSender(sender);
        } else if(param == "RECEIVER") {
            for(int i=0; i<it->second.getSize(); i++)
            {
                fa::AgentID receiver = fa::AgentID(it->second.entries[i]);
                aclMSG->addReceiver(receiver);
            }
        } else if(param == "REPLY-TO") {
            for(int i=0; i<it->second.getSize(); i++)
            {
                fa::AgentID reply_to = fa::AgentID(it->second.entries[i]);
                aclMSG->addReplyTo(reply_to);
            }
        } else if(param == "CONTENT") {
            std::string content = it->second.entries[0];
            for(int i=1; i<it->second.getSize(); i++)
            {
                content += " " + it->second.entries[i];
            }
            aclMSG->setContent(content);
        } else if(param == "LANGUAGE") {
            aclMSG->setLanguage(it->second.entries[0]);
        } else if(param == "ENCODING") {
            aclMSG->setEncoding(it->second.entries[0]);
        } else if(param == "ONTOLOGY") {
            aclMSG->setOntology(it->second.entries[0]);
        } else if(param == "PROTOCOL") {
            aclMSG->setProtocol(it->second.entries[0]);
        } else if(param == "CONVERSATION-ID") {
            aclMSG->setConversationID(it->second.entries[0]);
        } else if(param == "REPLY-WITH") {
            aclMSG->setReplyWith(it->second.entries[0]);
        } else if(param == "IN-REPLY-TO") {
            aclMSG->setInReplyTo(it->second.entries[0]);
        } else if(param == "REPLY-BY") {
            if(aclMSG->setReplyBy1(it->second.entries[0]) != 0)
            {
                return false;
            }
        } else {
            return false;
        }        
    }
    return true;
}
} // namespace root
