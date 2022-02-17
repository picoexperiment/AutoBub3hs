#include "Parser.hpp"

Parser::Parser(std::string RunFolder, std::string ImageFolder, std::string ImageFormat){
    this->RunFolder = RunFolder;
    this->ImageFolder = ImageFolder;
    this->ImageFormat = ImageFormat;

    this->StatusCode = 0;
}

Parser::~Parser(){}
