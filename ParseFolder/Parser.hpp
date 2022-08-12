/* Abstract base class for raw and zip parsers */
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#ifndef PARSER_HPP
#define PARSER_HPP

class Parser{
    private:

    protected:
        std::string RunFolder;
        std::string ImageFolder;
        std::string ImageFormat;
        int StatusCode;

    public:
        Parser(std::string, std::string, std::string);
        virtual ~Parser() = 0;

        virtual Parser* clone() = 0;

        virtual int GetImage(std::string, std::string, cv::Mat&) = 0;
        virtual void GetEventDirLists(std::vector<std::string>&) = 0;
        virtual void GetFileLists(const char*, std::vector<std::string>&, const char*) = 0;
        virtual void ParseAndSortFramesInFolder(std::string, int, std::vector<std::string>&) = 0;

};

#endif
