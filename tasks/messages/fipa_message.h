/*
 * \file    fipa_message.h
 *  
 * \brief   Generates bit-efficient fipa messages. 
 *
 * \details 
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    27.09.2010
 *        
 * \version 0.1 
 *          
 *
 * \author  Stefan.Haase@dfki.de
 */

#ifndef MODULES_ROOT_FIPAMESSAGE_HPP
#define MODULES_ROOT_FIPAMESSAGE_HPP

#include<iostream>
#include<boost/tokenizer.hpp>
#include<string>

#include "message_interface.h"
#include <message-generator/ACLMessage.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

namespace root
{
/**
 * 
 */
class FipaMessage : public MessageInterface
{
 public:
    FipaMessage() : MessageInterface(), msg(NULL)
    {
    };

    /**
     * Clears all fields.
     */
    void clear()
    {
        if(msg != NULL)
        {
            delete msg;
            msg = NULL;
        }
    }

    /**
     * Returns the message in the desired format (e.g. fipa acl).
     */
    std::string generateMessage() // virtual
    {
        fipa::acl::ACLMessageOutputParser generator = fipa::acl::ACLMessageOutputParser();
        generator.setMessage(msgACL);
        return generator.getBitMessage();
    }

    /**
     * Defines the message like: "SENDER mod1 RECEIVER mod2 RECEIVER mod3 CONTENT...".
     * The message is parsed by the function "parseMessage". 
     */
    bool setMessage(std::string fields) // virtual
    {
        if(msg == NULL)
        {
            msg = new ACLMessage();
        }
        fieldStr = fields;        

        // Split message into tokens.
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

        boost::char_separator<char> sep(" ");
        tokenizer tokens(fields, sep);
        for (tokenizer::iterator tok_iter = tokens.begin(); 
            tok_iter != tokens.end(); ++tok_iter)
        {
            std::string param = *tok_iter;
            ++tok_iter();
            if(tok_iter == tokens.end())
                return false;
            std::string value = *tok_iter;
            
            if(param == "PERFORMATIVE") {
                if(setPerformativ(value) != 0) {
                    log(RTT::Error) << "Performatives could not be set." << RTT::endlog();
                    return false;
                }
            } else if(param == "SENDER") {
                fipa::acl::AgentID sender = fipa::acl::AgentID(value);
                msgACL->setSender(sender);
            } else if(param == "RECEIVER") {
                fipa::acl::AgentID receiver = fipa::acl::AgentID(value);
                msgACL->addReceiver(receiver);
            } else if(== "REPLY-TO") {
                fipa::acl::AgentID reply_to = fipa::acl::AgentID(value);
                msgACL->addReplyTo(reply_to);
            } else if(== "CONTENT") {
                msgACL->setContent(value);
            } else if(== "LANGUAGE") {
                msgACL->setLanguage(value);
            } else if(== "ENCODING") {
                msgACL->setEncoding(value);
            } else if(== "ONTOLOGY") {
                msgACL->setOntology(value);
            } else if(== "PROTOCOL") {
                msgACL->setProtocol(value);
            } else if(== "CONVERSATION-ID") {
                msgACL->setConversationID(value);
            } else if(== "REPLY-WITH") {
                msgACL->setReplyWith(value);
            } else if(== "IN-REPLY-TO") {
                msgACL->setInReplyTo(value);
            } else if(== "REPLY-BY") {
                stringstream sstream(value);
                sstream.exceptions (ifstream::eofbit | )
                try {
                    long val_l = 0;
                    sstream >> val_l;
                } catch (ifstream::failure e) {
                    log(RTT::Error) << "Exception opening/reading file: " << 
                            e.what() << RTT::endlog();
                    return false;
                }
                msgACL->setReplyBy(val_l);
            }
        }
    }

 private:
    ACLMessage* msgACL;
    std::string fieldStr;

    DISALLOW_COPY_AND_ASSIGN(MessageInterface);
};
} // namespace root

#endif
