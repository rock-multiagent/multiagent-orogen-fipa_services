#include "message_interface.h"

namespace root
{
////////////////////////////////////////////////////////////////////
//                           PUBLIC                               //
////////////////////////////////////////////////////////////////////
void MessageInterface::clear(std::string const parameter_names)
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

int MessageInterface::getEntriesP(std::string const parameter_name, 
        std::vector<std::string>** entries)
{        
    std::map<std::string, MessageParameter>::iterator it;
    it = parameters.find(parameter_name);
    // Parameter available?
    if(it != parameters.end()) {
        *entries = &(it->second.entries);
    } else {
        entries = NULL;
        return -1;
    }

    int size = it->second.entries.size();
    // Entries all valid?
    bool valid = true;
    for(int i=0; i<size; i++)
    {
        if(it->second.entries.empty())
        {
            valid = false;
            break;
        }
    }
    if(!valid)
        return -2;

    // Entries available?
    return size;
}

bool MessageInterface::setMessage(std::string input)
{
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(" ");
    tokenizer tokens(input, sep);
    for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
    {
        std::string param = *tok_iter; 
        ++tok_iter;
        if(tok_iter == tokens.end()) return false;
        std::string entry = *tok_iter;

        // Get parameter.
        std::map<std::string, MessageParameter>::iterator it = parameters.find(param);
        if(it == parameters.end()) // Unknwon parameter.
        {
            return false;
        }
        if(entry == "START") // Add all following entries until STOP.
        {
            for(++tok_iter; tok_iter != tokens.end() && *tok_iter != "STOP" ; ++tok_iter)
            {
                it->second.addEntry(*tok_iter);
            }
        } 
        else // Just add the single entry. 
        {
            it->second.addEntry(*tok_iter);
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////
//                           PROTECTED                            //
////////////////////////////////////////////////////////////////////
MessageInterface::MessageInterface(std::string* parameter_names, int const noParas)
{
    for(int i=0; i<noParas; i++)
    {
        // Creates the map-entry <parametername, MessageParameter>.
        std::string name = parameter_names[i];
        parameters.insert(std::pair<std::string, MessageParameter>(name, MessageParameter(name)));
    }
}
} // namespace root
