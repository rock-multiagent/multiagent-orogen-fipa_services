#include "message_interface.h"

#include <iostream>
#include <stdio.h>

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
void MessageInterface::clear(std::string const& parameter_names)
{
    // Clear all entries.
    if(parameter_names == "")
    {
        std::map<std::string, MessageParameter>::iterator it;
        for(it=parameters.begin(); it != parameters.end(); ++it)
        {
            // Clear entry vector of the parameter.
            it->second.entries.clear();
        }
        return;
    }

     // Else: Delete special parameter-entries: Split message into tokens.
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(" ");
    tokenizer tokens(parameter_names, sep);
    for(tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
    {
        // Delete the entries of the specific parameter (if it exists).
        std::map<std::string, MessageParameter>::iterator it = parameters.find(*tok_iter);
        if(it != parameters.end())
            it->second.entries.clear();
    }
}

std::vector<std::string>& MessageInterface::getEntry(std::string const& parameter_name)
{        
    std::map<std::string, MessageParameter>::iterator it;
    it = parameters.find(parameter_name);
    // Parameter not available?
    if(it == parameters.end()) {
        throw MessageException("Parameter '" + parameter_name + "' unknown.");
    }

    // Entries not empty?
    if(it->second.entries.empty())
    {
        throw MessageException("Entry of parameter '" + 
                parameter_name + "' is empty.");
    }

    return it->second.entries;
}

std::string MessageInterface::getFirstEntry(std::string const& parameter_name)
{
    std::vector<std::string>& entries = getEntry(parameter_name);
    return entries.at(0);
}

void MessageInterface::setMessage(std::string const& input)
{
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(" ");
    tokenizer tokens(input, sep);
    for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
    {
        std::string param = *tok_iter; 
        ++tok_iter;
        if(tok_iter == tokens.end())
            throw MessageException("Wrong input format.");
        std::string entry = *tok_iter;

        // Get parameter.
        std::map<std::string, MessageParameter>::iterator it = parameters.find(param);
        if(it == parameters.end()) // Unknwon parameter.
        {
            throw MessageException("Parameter '" + param + "' unknown.");
        }
        if(entry == "BEGIN") // Add all following entries until END.
        {
            bool stop_found = false;
            for(++tok_iter; tok_iter != tokens.end(); ++tok_iter)
            {
                if(*tok_iter == "END")
                {
                    stop_found = true;
                    break;
                }
                it->second.addEntry(*tok_iter);
            }
            if(!stop_found)
                throw MessageException("Wrong input format.");
        } 
        else // Just add the single entry. 
        {
            it->second.addEntry(*tok_iter);
        }
    }
}

////////////////////////////////////////////////////////////////////
//                           PROTECTED                            //
////////////////////////////////////////////////////////////////////
MessageInterface::MessageInterface(std::string const* parameter_names, int const noParas)
{
    for(int i=0; i<noParas; i++)
    {
        // Creates the map-entry <parametername, MessageParameter>.
        std::string name = parameter_names[i];
        parameters.insert(std::pair<std::string, MessageParameter>(name, MessageParameter(name)));
    }
}
} // namespace root
