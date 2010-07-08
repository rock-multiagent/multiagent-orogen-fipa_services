#include "ModuleID.hpp"

namespace modules
{
bool ModuleID::splitID(std::string module_id,
        std::string* envID,
        std::string* type,
        std::string* module_name, 
        std::string tokens)
{
    char* pch = NULL;
    const char* tok = tokens.c_str();
    // Splits up the ID. If the ID has a wrong format, empty strings will be created.
    char mod_id[128];
    strcpy(mod_id, module_id.c_str());
    *envID = std::string((pch=strtok(mod_id, tok)) ? pch : "");
    *type = std::string((pch=strtok(NULL, tok)) ? pch : "");
    *module_name = std::string((pch=strtok(NULL, tok)) ? pch : "");
    return (!envID->empty() && !type->empty() && !module_name->empty()); 
}

std::string ModuleID::getEnvID(std::string module_id, std::string tokens)
{
    std::string envID;
    std::string type;
    std::string module_name;
    splitID(module_id, &envID, &type, &module_name, tokens);
    return envID;
}

std::string ModuleID::getType(std::string module_id, std::string tokens)
{
    std::string envID;
    std::string type;
    std::string module_name;
    splitID(module_id, &envID, &type, &module_name, tokens);
    return type;
}

std::string ModuleID::getModuleName(std::string module_id, std::string tokens)
{
    std::string envID;
    std::string type;
    std::string module_name;
    splitID(module_id, &envID, &type, &module_name, tokens);
    return module_name;
}
} // namespace modules
