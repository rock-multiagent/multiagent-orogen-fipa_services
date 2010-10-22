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
#include <string>
#include <vector>

#include<boost/tokenizer.hpp>

namespace root
{
/**
 * This abstract class is used to work with different kinds of message formats.
 * It allows to define a message using a simple string format which contains
 * <parameter entry> pairs. The inherited classes takes care of encoding and decoding of the 
 * stored informations. encodeMessage() returns the encoded message as a byte-string.
 * decodeMessage() decodes the message and fills the parameter-map.
 */
class MessageInterface
{
 public:
    struct MessageParameter
    {
        MessageParameter(std::string name)
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
    void clear(std::string const parameter_names="");

    /** 
     * Should decode the message and fills the parameter-map.
     */
    virtual bool decodeMessage(std::string& message) = 0;

    /**
     * Uses the entries in the parameters-map and returns the encoded 
     * message as a byte-string.
     */
    virtual std::string encodeMessage() = 0;

    /**
     * Use to request the parameter entries.
     * \param parameter_name Name of the parameter, depend on the used language.
     * \param parameters Passed pointer gets the address of the entry-vector, so the values kann
     * be modified directly.
     * \return -1 if the parameter is unknown, -2 if one of the entries is empty.
     * Otherwise the number of the entries is returned.
     */
    int getEntriesP(std::string const parameter_name, std::vector<std::string>** entries);

    /**
     * Parses the input string and fills the parameters-map.
     * Format: <PARAMETER entry> || <PARAMETER START entry* STOP> \n
     * START and STOP groups values together.
     * E.g.: <i> PERFORMATIVE inform SENDER mod1 RECEIVER START mod2 mod3 STOP </i>\n
     * Look at the class description of each supported language for further informations.
     */
    bool setMessage(std::string input);  

 protected:
    /**
     * Defines the valid parameter names.
     */
    MessageInterface(std::string* parameters, int const noParas);

 protected:
    std::map<std::string, MessageParameter> parameters; 
};
} // namespace root

#endif
