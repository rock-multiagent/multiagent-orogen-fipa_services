/*
 * \file    ModuleID.hpp
 *  
 * \brief   Class to split the module ID into ENVID, TYPE, NAME.
 *
 * \details Class is used to extract the information of the module id.
 *          These are the environmental ID (unique for every pc), the
 *          type of the module (at the moment CHAT, LOG, MTA, PROXY, ROOT),
 *          and for non-MTAs the name. The whole module ID has to be unique
 *          within all modules.
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    13.07.2010
 *
 * \author  Stefan.Haase@dfki.de
 */
#ifndef MODULES_ROOTMODULE_MODULEID_HPP
#define MODULES_ROOTMODULE_MODULEID_HPP

#include <string.h>

#include <string>

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);             \
    void operator=(const TypeName&)

namespace modules
{
/**
 * The ID (name of the module) of each module is build up like ENVID-TYPE-NAME.
 * The name of the message transport agents is ENVID-TYPE.
 * Each ID has to be unique within the framework.
 * - ENVID: Environment ID. Every module on one pc should have the same ENVID.
 * - TYPE: Type of the module.  At the moment: CHAT, LOG, MTA, PROXY, ROOT
 * - NAME: Name of the module. This field is empty for all MTAs 
 *         (because there is only one MTA in each environment).
 * This class will be extended with an xml file, where all the available
 * types and their abilities are listed.
 */
class ModuleID
{
 public:
    /**
     * Splits up the passed 'module_id' in the 'envID', 'type' and 'module_name'
     * (pointers must not be NULL). 'tokens' contains the separation-chars,
     * default is '_'. Use this method if you need all parts of the module-id. 
     * If the id could not be passed, empty strings will be set.
     */
    static bool splitID(std::string module_id,
            std::string* envID,
            std::string* type,
            std::string* module_name, 
            std::string tokens_="_");

    static std::string getEnvID(std::string module_id, std::string tokens="_");

    static std::string getType(std::string module_id, std::string tokens="_");

    static std::string getModuleName(std::string module_id, std::string tokens="_");

 private:
    ModuleID(){};
};
} // namespace modules
#endif
