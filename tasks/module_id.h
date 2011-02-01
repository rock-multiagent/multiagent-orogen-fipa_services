/*
 * \file    ModuleID.hpp
 *  
 * \brief   Class to split the module ID into ENVID and NAME.
 *
 * \details Class is used to extract the information of the module id.
 *          These are the environmental ID (unique for every pc) and the
 *          name of the module (e.g. MTA or ROOT01).
 *          MTAs must be named MTA. The whole module ID has to be unique
 *          within all modules.
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    01.02.2011
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

namespace root
{
/**
 * The ID (name of the module) of each module is build up like ENVID_NAME.
 * The name of the message transport agents is ENVID_TYPE.
 * Each ID has to be unique within the framework.
 * - ENVID: Environment ID. Every module on one pc should have the same ENVID.
 *          Attention: Contains an underscore, e.g. sherpa_0.
 * - NAME: Name appendix of the module. This field is expected to be 'MTA' for all MTAs 
 *         (because there is expected only one MTA in each environment).
 */
class ModuleID
{
 public:
    ModuleID(std::string const& module_id)
    {
        mID = module_id;
        //splitID(module_id, &mEnvID, &mName);
        // Second '_' divides the environment name and the appendix.
        int pos_sec__ = -1;
        for(int i=0; i<module_id.size(); i++)
        {
            if(module_id.at(i) == '_' && pos_sec__ == -1)
                pos_sec__ = 0;
            else if(module_id.at(i) == '_' && pos_sec__ == 0)
                pos_sec__ = i;
        }
        if(pos_sec__ > 0) // Find two substrings?
        {
            mEnvID = module_id.substr(0,pos_sec__);
            if(module_id.size() > pos_sec__ + 1)
            {
                int pos_name = pos_sec__ + 1;
                mName = module_id.substr(pos_name, module_id.size()-pos_name);
            }
        }
    }

    /**
     * Splits up the passed 'module_id' in the 'envID', 'type' and 'module_name'
     * (pointers must not be NULL). 'tokens' contains the separation-chars,
     * default is '_'. Use this method if you need all parts of the module-id. 
     * If the id could not be passed, empty strings will be set.
     */
    /*
    bool splitID(std::string const& module_id,
            std::string* envID,
            std::string* type,
            std::string* name, 
            std::string tokens_="_");
    */    

    inline std::string getID(){return mID;}
    inline std::string getEnvID(){return mEnvID;}
    inline std::string getName(){return mName;}

 private:
    std::string mID;
    std::string mEnvID;
    std::string mName;
};
} // namespace modules
#endif
