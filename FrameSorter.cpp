#include <string>
#include <cstdio>
#include <cassert>
#include "FrameSorter.hpp"

FrameSorter::FrameSorter(std::string ImageFormat){
    this->ImageFormat = ImageFormat;
}

bool FrameSorter::operator()(std::string i, std::string j){
    unsigned int sequence_i, camera_i;
    int got_i = sscanf(i.c_str(), this->ImageFormat.c_str(),
            &camera_i, &sequence_i);
    assert(got_i == 2);

    unsigned int sequence_j, camera_j;
    int got_j = sscanf(j.c_str(), this->ImageFormat.c_str(),
            &camera_j, &sequence_j);
    assert(got_j == 2);

    return sequence_i < sequence_j;
}
