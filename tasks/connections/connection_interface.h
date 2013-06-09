/*
 * \file    connection_interface.h
 *  
 * \brief   
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

#ifndef MTS_CONNECTIONINTERFACE_HPP
#define MTS_CONNECTIONINTERFACE_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include <base/time.h>
#include <fipa_acl/fipa_acl.h>

namespace mts
{
class ConnectionException : public std::runtime_error
{
 public:
    ConnectionException(const std::string& msg) : std::runtime_error(msg)
    {
    }
};

class InvalidService : public std::runtime_error
{
public:
    InvalidService(const std::string& msg) : std::runtime_error(msg)
    {}
};

/**
 * Connection interface between different modules
 * and betweeen modules and socket servers.
 *
 * \todo Find a way to use globalLogging.
 */
class ConnectionInterface
{
 public:
    virtual ~ConnectionInterface(){};

    /**
     * Retrieve reader part of this connection
     */
    virtual std::string getReceiverName() const { return mReceiverName; }

    /**
     * Retrieve writer part of this connection
     */
    virtual std::string getSenderName() const { return mSenderName; }

    /**
     * Establish connection
     */
    virtual bool connect()=0;

    /**
     * Close connection
     */
    virtual bool disconnect()=0;

    /**
     * Perform initialization of the connection if needed
     */
    inline bool initialize() { return true; };

    /**
     * Test whether a connection exists
     */
    inline bool isConnected() { return mConnected; };

    /**
     * Read a FIPA letter from the client
     */
    virtual fipa::acl::Letter read()=0;

    /**
     * Send a FIPA letter to the client
     */
    virtual bool send(const fipa::acl::Letter& data, fipa::acl::representation::Type representation)=0;

    /**
     * Inform the remote end about an update list of clients
     */
    virtual void updateAgentList(const std::vector<std::string>& agents) = 0; // virtual

    /**
     * Mark the update of the associated agents list
     */
    void setUpdateTime(const base::Time& time) { mUpdateTime = time; }

    /**
     * Get the 
     */
    base::Time getUpdateTime() const { return mUpdateTime; }

    /** Allow connections to store information about associated agents that can be reached via this connection
     * Set associated agents
     * \param agents Set of agents associated with this connection
     */
    void setAssociatedAgents(std::vector<std::string> agents) { mAssociatedAgents = agents; }

    /**
     * Get agents associated with this connection
     */
    std::vector<std::string> getAssociatedAgents() const { return mAssociatedAgents; }


    /**
     * Test whether a specific agent is attached to this connection, i.e. reachable
     */
    bool isAssociatedAgent(const std::string& name) const { return std::find(mAssociatedAgents.begin(),mAssociatedAgents.end(),name) != mAssociatedAgents.end(); }

 protected:
    ConnectionInterface(){};

    std::string mSenderName;
    std::string mReceiverName;
    bool mConnected;

    base::Time mUpdateTime;
    // Agents that can be reached via this connection
    std::vector<std::string> mAssociatedAgents;
};
} // namespace mts

#endif
