#include "Parser.hpp"

#ifndef RAW_PARSER_HPP
#define RAW_PARSER_HPP

class RawParser: public Parser {
    private:

    protected:

    public:
        RawParser(std::string, std::string, std::string);
        ~RawParser();

        void GetImage(int, int, int, cv::Mat&) override;
        void GetEventDirLists(std::vector<std::string>&) override;
        void GetFileLists(const char*, std::vector<std::string>&, const char*) override;

};

#endif
