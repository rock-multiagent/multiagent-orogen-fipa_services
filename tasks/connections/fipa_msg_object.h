#ifndef MODULES_RIMRES_BASE_DATA_H_
#define MODULES_RIMRES_BASE_DATA_H_

#ifndef __orogen
#include <stdint.h>

#include <string>
#include <vector>
#include <base/time.h>
#endif

namespace fipa {

struct BitefficientMessage {
#ifndef __orogen
 public:
    BitefficientMessage()
        : time(base::Time::now())
    {
    }

    BitefficientMessage(const std::string& msg)
        : time(base::Time::now())
    {
        push_back(msg);
    }

    ~BitefficientMessage()
    {
        data.clear();
    }

    inline void clear()
    {
        data.clear();
    }

    inline std::vector<uint8_t> getVector() const 
    {
        return this->data;
    }

    inline void push_back(uint8_t element)
    { 
        this->data.push_back(element);
    }

    inline void push_back(std::string msg)
    {
        for(unsigned int i=0; i<msg.size(); i++)
        {
            this->data.push_back(msg[i]);
        } 
    }

    inline int size() const 
    {
        return this->data.size(); 
    }

    inline std::string toString() const 
    {
	    std::string str = "";
	    for (std::vector<uint8_t>::const_iterator it = data.begin(); it != data.end(); it++)
		    str += (*it);
	    return str;
    }

    #endif
    std::vector<uint8_t> data;
    base::Time time;
};

} // namespace modules
#endif

