#ifndef MODULES_RIMRES_BASE_DATA_H_
#define MODULES_RIMRES_BASE_DATA_H_

#ifndef __orogen
#include <stdint.h>

#include <string>
#include <vector>
#endif

namespace modules {

struct Vector {
#ifndef __orogen
 public:
    Vector()
    {
    }

    Vector(std::string msg)
    {
        push_back(msg);
    }

    ~Vector()
    {
        vec.clear();
    }

    inline void clear()
    {
        vec.clear();
    }

    inline std::vector<uint8_t> getVector() const 
    {
        return this->vec;
    }

    inline void push_back(uint8_t element)
    { 
        this->vec.push_back(element);
    }

    inline void push_back(std::string msg)
    {
        for(int i=0; i<msg.size(); i++)
        {
            this->vec.push_back(msg[i]);
        } 
    }

    inline int size() const 
    {
        return this->vec.size(); 
    }

    inline std::string toString() const 
    {
	    std::string str = "";
	    for (std::vector<uint8_t>::const_iterator it = vec.begin(); it != vec.end(); it++)
		    str += (*it);
	    return str;
    }

    #endif
    std::vector<uint8_t> vec;
};
} // namespace modules
#endif

