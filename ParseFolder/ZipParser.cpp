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
#include <istream>
#include <string.h>
#include <sys/types.h> //.... added
#include <sys/stat.h>  //....

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#include "../minizip-ng/mz.h"
#include "../minizip-ng/mz_zip.h"
#include "../minizip-ng/mz_strm.h"
#include "../minizip-ng/mz_strm_os.h"
#include "../minizip-ng/mz_strm_buf.h"
#include "../minizip-ng/mz_strm_split.h"
#include "../minizip-ng/mz_zip_rw.h"

#define debug false
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

    /* All minizip functions which return ints are zero for
     * success, or negative.  So we keep adding to err and
     * check at the end if it's equal to 0. */
    int err = 0;

    /* Create buffers, open zip file.
     * This is taken almost verbatim from minizip-ng/mz_zip_reader.c.
     */
    mz_stream_os_create(&this->file_stream);
    mz_stream_buffered_create(&this->buff_stream);
    mz_stream_split_create(&this->split_stream);

    err += mz_stream_set_base(this->buff_stream, this->file_stream);
    err += mz_stream_set_base(this->split_stream, this->buff_stream);
    err += mz_stream_open(this->split_stream, this->RunFolder.c_str(), MZ_OPEN_MODE_READ);

    mz_zip_create(&this->zip_handle);
    err += mz_zip_open(this->zip_handle, this->split_stream, MZ_OPEN_MODE_READ);

    if (err != MZ_OK){
        std::cerr << "Error initializing zip file; cannot continue" << std::endl;
        std::cerr << " Error code: " << err << std::endl;
        throw -10;
    }

    this->runID = "";
    this->runFileLoc = -1;
}


/* Destructor
 * Release the zip_reader object
 */
ZipParser::~ZipParser(){
    mz_zip_close(this->zip_handle);
    mz_zip_delete(&this->zip_handle);
    mz_stream_split_close(this->split_stream);
    mz_stream_split_delete(&this->split_stream);
    mz_stream_buffered_close(this->buff_stream);
    mz_stream_buffered_delete(&this->buff_stream);
    mz_stream_os_close(this->file_stream);
    mz_stream_os_delete(&this->file_stream);
}


/* Clone
 * Create a new instance of a ZipParser with all the same info as this one.
 */
ZipParser* ZipParser::clone(){
    ZipParser* other = new ZipParser(this->RunFolder, this->ImageFolder, this->ImageFormat);

    other->FileContents = this->FileContents;
    other->ImageNames = this->ImageNames;
    other->ImageLocs = this->ImageLocs;

    return other;
}


void ZipParser::BuildFileList(){
    auto t0 = std::chrono::high_resolution_clock::now();

    /* Build a vector with the file contents */
    std::string re_str = "^.*/(\\d+)/.*/?(cam\\d.*image\\s*\\d+.*(png|bmp))";
    boost::regex re(re_str);
    boost::smatch match;

    /* Search for run file */
    std::string run_id_str = ".*(\\d{8}_\\d+).*";
    boost::regex run_id_regex(run_id_str);
    std::string run_file_str;
    boost::regex run_file_regex;

    int err = 0;
    mz_zip_file *file_info = NULL;

    err = mz_zip_goto_first_entry(this->zip_handle);

    /* Retreive event file name and location from entries in zip file.
     * The regex fills the match object with the event and file name in the
     * [1] and [2] place.
     */
    while (err == MZ_OK){
        err = mz_zip_entry_get_info(this->zip_handle, &file_info);
        this->FileContents.push_back(file_info->filename);
        std::string filename = file_info->filename;

        // Check match for images
        if (boost::regex_match(filename, match, re)){
            this->ImageNames[match[1].str()][match[2].str()] = file_info->filename;
            this->ImageLocs[match[1].str()][match[2].str()] = mz_zip_get_entry(this->zip_handle);

        }

        // Try to find run ID
        if (this->runID.empty() && boost::regex_match(filename, match, run_id_regex)){
            boost::filesystem::path _tmp = match[1].str();
            _tmp += "/";  // This ensures the last character is a slash, required for the next part
            this->runID = _tmp.parent_path().filename().string();
            run_file_str = ".*" + this->runID + ".txt";
            run_file_regex = boost::regex(run_file_str);
        }

        // Just double check if it's filled from the last if statement.
        // CANNOT happen in an else, or it might miss the file.
        if (!this->runID.empty() && (this->runFileLoc < 0)) {
            if (boost::regex_match(filename, match, run_file_regex)){
                this->runFileLoc = mz_zip_get_entry(this->zip_handle);
            }
        }
        err = mz_zip_goto_next_entry(this->zip_handle);
    }
    if (err != MZ_END_OF_LIST){
        std::cout << "Error when creating file list; cannot continue." << std::endl;
        throw -10;
    }

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
int ZipParser::GetImage(std::string EventID, std::string FrameName, cv::Mat &Image){
    if (this->FileContents.size()==0) this->BuildFileList();
    auto t0 = std::chrono::high_resolution_clock::now();

    int err = 0;

    // Check to make sure the element is in the list
    if (
            this->ImageLocs.find(EventID) == this->ImageLocs.end() ||
            this->ImageLocs[EventID].find(FrameName) == this->ImageLocs[EventID].end()
       )
    {
        return -1;
    }


    /* Go to the location of the data in the zip file, get file info. */
    mz_zip_file *file_info;
    err += mz_zip_goto_entry(this->zip_handle, this->ImageLocs[EventID][FrameName]);
    err += mz_zip_entry_get_info(this->zip_handle, &file_info);

    auto t03 = std::chrono::high_resolution_clock::now();
    /* Store data from file in buffer */
    char buf[file_info->uncompressed_size];
    err += mz_zip_entry_read_open(this->zip_handle, 0, NULL);
    mz_zip_entry_read(this->zip_handle, buf, file_info->uncompressed_size);
    err += mz_zip_entry_close(this->zip_handle);

    if (debug){
        auto t13 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt3 = t13 - t03;
        std::cout << "GetImage->save_to_buffer: " << dt3.count() << std::endl;
    }

    /* Reinterpret buffer as OpenCV Mat object */
    cv::Mat rawData(1, file_info->uncompressed_size, CV_8U, (void*) buf);
    Image = cv::imdecode(rawData, 0);

    if (debug){
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt = t1 - t0;
        std::cout << "GetImage: " << dt.count() << std::endl;
    }

    // err is 0 if everything is OK, nonzero otherwise.
    // Return !err so is is true if success.
    return !err;
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


void ZipParser::GetRunFileInfo(std::vector<std::string>& EventListFromFile){
    if (this->FileContents.size()==0) this->BuildFileList();

    int err = 0;

    /* Go to the location of the data in the zip file, get file info. */
    mz_zip_file *file_info;
    err += mz_zip_goto_entry(this->zip_handle, this->runFileLoc);
    err += mz_zip_entry_get_info(this->zip_handle, &file_info);

    /* Store data from file in buffer */
    char buf[file_info->uncompressed_size];
    err += mz_zip_entry_read_open(this->zip_handle, 0, NULL);
    mz_zip_entry_read(this->zip_handle, buf, file_info->uncompressed_size);
    err += mz_zip_entry_close(this->zip_handle);

    std::istringstream iss(buf);

    // Extract actual event numbers from run file
    std::string _;
    int eventNum;
    while (iss >> _ >> eventNum >> _ >> _ >> _ >> _ >> _ >> _ >> _ >> _ >> _){
        EventListFromFile.push_back(std::to_string(eventNum));
    }
}
