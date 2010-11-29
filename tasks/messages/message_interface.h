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
 * \author  Stefan.Haase@dfki.de
 */

#ifndef MODULES_ROOT_MESSAGES_INTERFACE_HPP
#define MODULES_ROOT_MESSAGES_INTERFACE_HPP

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include<boost/tokenizer.hpp>

namespace root
{
class MessageException : public std::runtime_error
{
 public:
    MessageException(const std::string& msg) : runtime_error(msg)
    {
    }
};

/**
 * This abstract class is used to work with different kinds of message formats.
 * It allows to define a message using a simple string format which contains
 * <parameter entry> pairs. The inherited classes takes care of encoding and decoding of 
 * the stored informations. encodeMessage() returns the encoded message as a byte-string.
 * decodeMessage() decodes the message and fills the parameter-map.
 */
class MessageInterface
{
 public:
    struct MessageParameter
    {
        MessageParameter(std::string const& name)
        {
            this->name = name;
        }
        inline void addEntry(std::string entry)
        {
            entries.push_back(entry);
        }
        inline void clearEntries()
        {
            entries.clear();
        }
        inline int getSize()
        {
            return entries.size();
        }
        inline std::string operator[](unsigned int pos)
        {
            if(pos < entries.size())
                return entries[pos];
            else 
                return ""; 
        } 
        std::string name;
        std::vector<std::string> entries;
    };

 public:
    virtual ~MessageInterface(){};

    /**
     * Clears all or specific entries in the map.
     * \param parameter_names Can be a space separated list of parameter-names,
     * whose entries should be cleared. If an empty string is passed
     * (default) all parameter-entries are removed.
     */
    void clear(std::string const& parameter_names="");

    /** 
     * Should decode the message and fills the parameter-map.
     */
    virtual void decode(std::string const& message) = 0;

    /**
     * Uses the entries in the parameters-map and returns the encoded 
     * message as a byte-string.
     */
    virtual std::string encode() = 0;

    /**
     * Use to request the parameter-entries.
     * Throws a MessageException if an error occurred.
     * \param parameter_name Name of the parameter, depend on the used language.
     * \return Pointer to the entry-vector.
     */
    std::vector<std::string>& getEntry(std::string const& parameter_name);

    /**
     * Parses the input string and fills the parameters-map.
     * Format: <PARAMETER entry> || <PARAMETER START entry* STOP> \n
     * START and STOP groups values together.
     * E.g.: <i> PERFORMATIVE inform SENDER mod1 RECEIVER START mod2 mod3 STOP </i>\n
     * Look at the class description of each supported language for further informations.
     * Throws MessageException.
     */
    void setMessage(std::string const& input);

    /**
     * Can be used to set the entries of a parameter directly.
     */
    virtual bool setParameter(std::string const& parameter, std::vector<std::string> const& entries)=0;

 protected:
    /**
     * Defines the valid parameter names.
     */
    MessageInterface(std::string const* parameters, int const noParas);

 protected:
    std::map<std::string, MessageParameter> parameters; 
};
} // namespace root

#endif
