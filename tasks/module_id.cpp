#include "module_id.h"

namespace root
{
/*
int max_length = 256;

bool ModuleID::splitID(std::string const& module_id,
        std::string* envID,
        std::string* type,
        std::string* name, 
        std::string tokens)
{
    mID = module_id;
    char* pch = NULL;
    const char* tok = tokens.c_str();
    // Splits up the ID. If the ID has a wrong format, empty strings will be created.
    char mod_id[max_length];
    strcpy(mod_id, module_id.c_str());
    *envID = std::string((pch=strtok(mod_id, tok)) ? pch : "");
    *type = std::string((pch=strtok(NULL, tok)) ? pch : "");
    *name = std::string((pch=strtok(NULL, tok)) ? pch : "");
    return (!envID->empty() && !type->empty() && !name->empty()); 
}
*/
} // namespace modules
