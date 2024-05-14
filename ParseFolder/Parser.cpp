#include "Parser.hpp"

Parser::Parser(std::string RunFolder, std::string ImageFolder, std::string ImageFormat){
    this->RunFolder = RunFolder;
    this->ImageFolder = ImageFolder;
    this->ImageFormat = ImageFormat;

    this->StatusCode = 0;
}

Parser::~Parser(){}

void Parser::VerifyEventList(std::vector<std::string>& EventList){
    std::vector<std::string> eventNums;
    this->GetRunFileInfo(eventNums);

    // Compare event list, remove elements not in eventNums
    // Loop over the EventList, remove if value not in eventNums
    for (std::vector<std::string>::iterator it = EventList.begin(); it != EventList.end();){
        if (std::find(eventNums.begin(), eventNums.end(), *it) == eventNums.end()){
            it = EventList.erase(it);
        } else {
            ++it;
        }
    }
}
