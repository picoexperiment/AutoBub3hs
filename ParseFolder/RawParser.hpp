#include "Parser.hpp"

#ifndef RAW_PARSER_HPP
#define RAW_PARSER_HPP

class RawParser: public Parser {
    private:

    protected:

    public:
        RawParser(std::string, std::string, std::string);
        ~RawParser();

        virtual RawParser* clone() override;

        int GetImage(std::string, std::string, cv::Mat&) override;
        void GetEventDirLists(std::vector<std::string>&) override;
        void GetFileLists(const char*, std::vector<std::string>&, const char*) override;
        void ParseAndSortFramesInFolder(std::string, int, std::vector<std::string>&) override;

        void GetRunFileInfo(std::vector<std::string>&) override;

};

#endif
