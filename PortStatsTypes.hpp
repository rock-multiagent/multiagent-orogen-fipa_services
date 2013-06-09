#ifndef CHANNEL_STATS_TYPES
#define CHANNEL_STATS_TYPES

#include <numeric/Stats.hpp>

namespace mts
{
     /**
     * Simple structure to collect some statistics about a port
     * regarding the number of messages treated per component update and overall
     */
     struct PortStats
     {
        std::string portName;
        double meanMessageNumberPerUpdate; 
        double stdevMessageNumberPerUpdate;
        double totalMessageNumber;

        PortStats(const std::string& _portName, const base::Stats<double>& messageNumberStats)
            : portName(_portName)
            , meanMessageNumberPerUpdate(messageNumberStats.mean())
            , stdevMessageNumberPerUpdate(messageNumberStats.stdev())
            , totalMessageNumber(messageNumberStats.n()*messageNumberStats.mean())
        {}
          

        PortStats()
            : portName("")
            , meanMessageNumberPerUpdate(0)
            , stdevMessageNumberPerUpdate(0)
            , totalMessageNumber(0)
        {}
     };
}
#endif // CHANNEL_STATS_TYPES
