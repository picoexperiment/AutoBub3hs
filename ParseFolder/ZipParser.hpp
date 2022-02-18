#include <map>
#include "Parser.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

#include "../minizip-ng/mz.h"
#include "../minizip-ng/mz_zip.h"
#include "../minizip-ng/mz_strm.h"
#include "../minizip-ng/mz_zip_rw.h"

#ifndef ZIP_PARSER_HPP
#define ZIP_PARSER_HPP

class ZipParser: public Parser {
    private:
        void *zip_reader;

        int lastImageLoc;

        void BuildFileList();
        int SearchForFile(int, int, std::string);

    protected:

    public:
        ZipParser(std::string, std::string, std::string);
        ~ZipParser();


        virtual ZipParser* clone() override;

        void GetImage(std::string, std::string, cv::Mat&) override;
        void GetEventDirLists(std::vector<std::string>&) override;
        void GetFileLists(const char*, std::vector<std::string>&, const char*) override;
        void ParseAndSortFramesInFolder(std::string, int, std::vector<std::string>&) override;

        std::vector<std::string> FileContents;
        /* Mapping of image names in zip file:
         *   string: EventID
         *   string: image name (e.g. cam0_image30.png)
         *   string: full name of image in zip file
         */
        std::map<std::string, std::map<std::string, std::string> > ImageNames;
};

#endif
