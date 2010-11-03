#include "module_id.h"

namespace root
{
int max_length = 128;

bool ModuleID::splitID(std::string const& module_id,
        std::string* envID,
        std::string* type,
        std::string* name, 
        std::string tokens)
{
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

std::string ModuleID::getEnvID(std::string const& module_id, std::string tokens)
{
    std::string envID;
    std::string type;
    std::string name;
    splitID(module_id, &envID, &type, &name, tokens);
    return envID;
}

std::string ModuleID::getType(std::string const& module_id, std::string tokens)
{
    std::string envID;
    std::string type;
    std::string name;
    splitID(module_id, &envID, &type, &name, tokens);
    return type;
}

std::string ModuleID::getName(std::string const& module_id, std::string tokens)
{
    std::string envID;
    std::string type;
    std::string name;
    splitID(module_id, &envID, &type, &name, tokens);
    return name;
}
} // namespace modules
