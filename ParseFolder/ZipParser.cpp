/* *****************************************************************************
 * This file contains the necessary functions for Parsing an event folder
 * ALGORITHM given a folder, finds the .jpg files and sorts them by time.
 *
 * by Pitam Mitra for the PICO Geyser ImageAnalysis Algorithm.
 *
 * Created: 1 Feb 2014
 *
 * Issues: None atm
 *
 *******************************************************************************/
#include "ZipParser.hpp"
#include <dirent.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <string.h>
#include <sys/types.h> //.... added
#include <sys/stat.h>  //....

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#include "mz.h"
#include "mz_zip.h"
#include "mz_strm.h"
#include "mz_zip_rw.h"

#define debug true
#include <chrono>
using std::chrono::milliseconds;


/* Constructor */
ZipParser::ZipParser(std::string RunFolder, std::string ImageFolder, std::string ImageFormat):
Parser(RunFolder, ImageFolder, ImageFormat){
    /* Remove the trailing slash if it exists, then add zip extension if required. */
    boost::filesystem::path p(this->RunFolder);
    p.remove_trailing_separator();
    if (p.extension() == "") p += ".zip";
    this->RunFolder = p.native();

    this->lastImageLoc = -1;

    mz_zip_reader_create(&this->zip_reader);
    mz_zip_reader_open_file(this->zip_reader, this->RunFolder.c_str());
}


/* Destructor
 * Release the zip_reader object
 */
ZipParser::~ZipParser(){
    mz_zip_reader_close(this->zip_reader);
    mz_zip_reader_delete(&this->zip_reader);
}


/* Clone
 * Create a new instance of a ZipParser with all the same info as this one.
 */
ZipParser* ZipParser::clone(){
    ZipParser* other = new ZipParser(this->RunFolder, this->ImageFolder, this->ImageFormat);

    other->FileContents = this->FileContents;
    other->ImageNames = this->ImageNames;

    return other;
}


void ZipParser::BuildFileList(){
    auto t0 = std::chrono::high_resolution_clock::now();
    /* Build a vector with the file contents */
    mz_zip_file *file_info = NULL;
    if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK){
        if (mz_zip_reader_entry_get_info(zip_reader, &file_info) == MZ_OK){
            this->FileContents.push_back(file_info->filename);
        }
        else {
            std::cout << "Could not get info for first file; exiting..." << std::endl;
            throw -1;
        }
    }
    else {
        std::cout << "Could not seek first file; exiting..." << std::endl;
        throw -1;
    }

    while (mz_zip_reader_goto_next_entry(zip_reader) == MZ_OK){
        if (mz_zip_reader_entry_get_info(zip_reader, &file_info) == MZ_OK){
            this->FileContents.push_back(file_info->filename);
        }
        else {
            std::cout << "Failed to get entry info after first; exiting..." << std::endl;
            throw -1;
        }
    }

    /* Capture event and file name info from FileContents.
     * The regex fills the match object with the event and file name in the
     * [1] and [2] place.
     */
    std::string re_str = "^.*/(\\d+)/.*/?(cam\\d.*image\\s*\\d+.*(png|bmp))";
    boost::regex re(re_str);
    boost::smatch match;

    for (int i=0; i<this->FileContents.size(); i++){
        if (boost::regex_match(this->FileContents[i], match, re)){
            this->ImageNames[match[1].str()][match[2].str()] = this->FileContents[i];
        }
    }

//    for (auto&& x: this->FileContents) std::cout << x << std::endl;
    if (debug){
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt = t1 - t0;
        std::cout << "BuildFileList: " << dt.count() << std::endl;
    }
}


int ZipParser::SearchForFile(int start, int end, std::string re_str){
    boost::regex re(re_str);

    for (int i=start; i<end; i++){
        if (boost::regex_match(this->FileContents[i], re)){
            std::cout << "found close" << std::endl;
            return i;
        }
    }
    return -1;
}


/* Return the image as cv::Mat */
void ZipParser::GetImage(std::string EventID, std::string FrameName, cv::Mat &Image){
    if (this->FileContents.size()==0) this->BuildFileList();
    auto t0 = std::chrono::high_resolution_clock::now();

    std::string imageLoc = this->ImageNames[EventID][FrameName];

    auto t02 = std::chrono::high_resolution_clock::now();
    /* Locate the entry, then retrieve data into buffer */
    mz_zip_reader_locate_entry(this->zip_reader, imageLoc.c_str(), 1);
    if (debug){
        auto t12 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt2 = t12 - t02;
        std::cout << "GetImage->locate_entry: " << dt2.count() << std::endl;
    }

    auto t03 = std::chrono::high_resolution_clock::now();

    int32_t buf_size = (int32_t) mz_zip_reader_entry_save_buffer_length(zip_reader);
    char *buf = new char[buf_size];
    int32_t err = mz_zip_reader_entry_save_buffer(zip_reader, buf, buf_size);

    if (debug){
        auto t13 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt3 = t13 - t03;
        std::cout << "GetImage->save_to_buffer: " << dt3.count() << std::endl;
    }

    if (err){
        std::cout << "Could not retrieve image data from zip file; exiting..." << std::endl;
    }

    auto t04 = std::chrono::high_resolution_clock::now();

    /* Reinterpret buffer as OpenCV Mat object */
    cv::Mat rawData(1, buf_size, CV_8U, (void*) buf);
    Image = cv::imdecode(rawData, 0);

    if (debug){
        auto t14 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt4 = t14 - t04;
        std::cout << "GetImage->imdecode: " << dt4.count() << std::endl;
    }

    delete buf;

    if (debug){
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt = t1 - t0;
        std::cout << "GetImage: " << dt.count() << std::endl;
    }
}

/*Function to Generate File Lists*/
void ZipParser::GetFileLists(const char* EventFolder, std::vector<std::string>& FileList, const char* camera_out_name)
{
}


/*Function to Generate File Lists*/
void ZipParser::GetEventDirLists(std::vector<std::string>& EventList){
    if (this->FileContents.size()==0) this->BuildFileList();
    auto t0 = std::chrono::high_resolution_clock::now();
    /* Regex should match each event folder */
    boost::regex expression("^.*/(\\d+)/$");
    boost::smatch what;

    for (int i=0; i < this->FileContents.size(); i++){
        if (boost::regex_match(this->FileContents[i], what, expression)){
            EventList.push_back(what[1].str());
        }
    }

    /*
    char eventdir_buf[100];

    int ev = 0;
    */
    //sprintf(eventdir_buf, "*/%d/", ev);
    /*
    mz_zip_reader_set_pattern(this->zip_reader, eventdir_buf, 1);

    while (mz_zip_reader_goto_first_entry(this->zip_reader) == MZ_OK){
        EventList.push_back(std::to_string(ev));

        ev++;
    */
        //sprintf(eventdir_buf, "*/%d/", ev);
    /*
        mz_zip_reader_set_pattern(this->zip_reader, eventdir_buf, 1);
    }
    */
    if (debug){
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt = t1 - t0;
        std::cout << "GetEventDirLists: " << dt.count() << std::endl;
    }
}

/* Retreive the image path names for a specific event */
void ZipParser::ParseAndSortFramesInFolder(std::string EventID, int camera, std::vector<std::string>& Contents){
    if (this->FileContents.size()==0) this->BuildFileList();
    auto t0 = std::chrono::high_resolution_clock::now();

    std::string re_str = "cam" + std::to_string(camera) + ".*image.*(png|bmp)";
    boost::regex re(re_str);

    for (auto&& entry: this->ImageNames[EventID]){
        if (boost::regex_match(entry.first, re)) Contents.push_back(entry.first);
    }

//    std::string re_str = ".*/" + EventID + "/.*cam" + std::to_string(camera) + ".*image.*(png|bmp)";
//    boost::regex re(re_str);

    /* Loop over file contents, and check against the regular expression for matching image files */
//    for (auto&& entry: this->FileContents){
//        if (boost::regex_match(entry, re)){
            /* For simplicity, a filesystem::path object is created to split the file name from the rest of the path */
//            boost::filesystem::path p(entry);
//            Contents.push_back(p.filename().native());
//        }
//    }
    std::sort(Contents.begin(), Contents.end());
    if (debug){
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt = t1 - t0;
        std::cout << "ParseAndSortFramesInFolder: " << dt.count() << std::endl;
    }
}
