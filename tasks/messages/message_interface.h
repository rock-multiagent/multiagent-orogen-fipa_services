/*
 * \file    message_interface.h
 *  
 * \brief   Interface to use different message formats.
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

#ifndef MODULES_ROOT_MESSAGEINTERFACE_HPP
#define MODULES_ROOT_MESSAGEINTERFACE_HPP

#include <string>
#include <vector>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

namespace root
{
/**
 * 
 */
class MessageInterface
{
 public:
    MessageInterface()
    {
        if (RTT::log().getLogLevel() < RTT::Logger::Info)
        {
            RTT::log().setLogLevel(RTT::Logger::Info);
        }
    };

    /**
     * Clears all fields.
     */
    virtual void clear();

    /**
     * Returns the message encoded as a string.
     */
    virtual std::string generateMessage();

    /**
     * Defines the message like: "SENDER mod1 RECEIVER mod2 RECEIVER mod3 CONTENT...".
     */
    virtual void setMessage(std::string fields);

 private:
    DISALLOW_COPY_AND_ASSIGN(MessageInterface);
};
} // namespace root

#endif
