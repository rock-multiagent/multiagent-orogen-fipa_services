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

#ifndef MODULES_ROOT_MESSAGES_FIPA_HPP
#define MODULES_ROOT_MESSAGES_FIPA_HPP

#include<iostream>
#include<boost/tokenizer.hpp>
#include<string>

#include "message_interface.h"
#include <message-generator/ACLMessage.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

namespace fa = fipa::acl;

namespace root
{
/**
 * PARAMETER can be: PERFORMATIVE, SENDER, RECEIVER, REPLY-TO, CONTENT, LANGUAGE, ENCODING, 
 * ONTOLOGY, PROTOCOL, CONVERSATION-ID, REPLY-WITH, IN-REPLY-TO, REPLY-BY. \n
 * E.g.: <i> PERFORMATIVE inform SENDER mod1 RECEIVER START mod2 mod3 STOP </i>\n
 * At the moment just the name-field of the AgentID is used. The field resolver
 * for example should get the MTA of the agent.\n
 * REPLY-BY format: 2010-12-23T23:12:45:100.
 */
class FipaMessage : public MessageInterface
{
 public:
    FipaMessage();

    /**
     * Decodes the message and clears and refills the parameter-map.
     */ 
    bool decodeMessage(std::string& message); // virtual

    /**
     * Returns the encoded message as a string.
     * Uses the entries stored in the parameter-map.
     */
    std::string encodeMessage(); // virtual

    inline fa::ACLMessage* getACLMessage()
    {
        return aclMSG;
    }

 private:
    /**
     * Fills the ACLMessage using the information of the parameters-map.
     */
    bool createACLMessage();

 private:
    fa::ACLMessage* aclMSG;

    DISALLOW_COPY_AND_ASSIGN(FipaMessage);
};
} // namespace root

#endif
