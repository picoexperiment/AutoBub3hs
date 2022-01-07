#ifndef FRAMESORTER_HPP
#define FRAMESORTER_HPP

#include <string>
#include <iostream>

class FrameSorter{
    private:
        std::string ImageFormat;
    public:
        FrameSorter(std::string );
        bool operator()( std::string, std::string );
};


#endif
