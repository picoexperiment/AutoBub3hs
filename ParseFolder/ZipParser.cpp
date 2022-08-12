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
#include "mz_strm_os.h"
#include "mz_strm_buf.h"
#include "mz_strm_split.h"
#include "mz_zip_rw.h"

#define debug false


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
        throw -10;
    }

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
    /* Build a vector with the file contents */
    std::string re_str = "^.*/(\\d+)/.*/?(cam\\d.*image\\s*\\d+.*(png|bmp))";
    boost::regex re(re_str);
    boost::smatch match;

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
        if (boost::regex_match(filename, match, re)){
            this->ImageNames[match[1].str()][match[2].str()] = file_info->filename;
            this->ImageLocs[match[1].str()][match[2].str()] = mz_zip_get_entry(this->zip_handle);
        }
        err = mz_zip_goto_next_entry(this->zip_handle);
    }
    if (err != MZ_END_OF_LIST){
        std::cout << "Error when creating file list; cannot continue." << std::endl;
        throw -10;
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


/* Retrieve an image from the event EventID with frame number FrameName,
 * and fill the Image argument with the image data.
 *
 * Returns:
 *   0:     success
 *   <0:    zip file error
 *   >0:    image data retrieved from zip file, but resulting image is
 *          empty after interpreting.
 */
int ZipParser::GetImage(std::string EventID, std::string FrameName, cv::Mat &Image){
    if (this->FileContents.size()==0) this->BuildFileList();

    int err = 0;

    /* Go to the location of the data in the zip file, get file info. */
    mz_zip_file *file_info;
    err += mz_zip_goto_entry(this->zip_handle, this->ImageLocs[EventID][FrameName]);
    err += mz_zip_entry_get_info(this->zip_handle, &file_info);

    /* Store data from file in buffer */
    char buf[file_info->uncompressed_size];
    err += mz_zip_entry_read_open(this->zip_handle, 0, NULL);
    mz_zip_entry_read(this->zip_handle, buf, file_info->uncompressed_size);
    err += mz_zip_entry_close(this->zip_handle);

    if (err) { return err; };

    /* Reinterpret buffer as OpenCV Mat object */
    cv::Mat rawData(1, file_info->uncompressed_size, CV_8U, (void*) buf);
    Image = cv::imdecode(rawData, 0);

    return Image.empty();
}

/*Function to Generate File Lists*/
void ZipParser::GetFileLists(const char* EventFolder, std::vector<std::string>& FileList, const char* camera_out_name)
{
}


/*Function to Generate File Lists*/
void ZipParser::GetEventDirLists(std::vector<std::string>& EventList){
    if (this->FileContents.size()==0) this->BuildFileList();
    /* Regex should match each event folder */
    boost::regex expression("^.*/(\\d+)/$");
    boost::smatch what;

    for (int i=0; i < this->FileContents.size(); i++){
        if (boost::regex_match(this->FileContents[i], what, expression)){
            EventList.push_back(what[1].str());
        }
    }
}

/* Retreive the image path names for a specific event */
void ZipParser::ParseAndSortFramesInFolder(std::string EventID, int camera, std::vector<std::string>& Contents){
    if (this->FileContents.size()==0) this->BuildFileList();

    std::string re_str = "cam" + std::to_string(camera) + ".*image.*(png|bmp)";
    boost::regex re(re_str);

    for (auto&& entry: this->ImageNames[EventID]){
        if (boost::regex_match(entry.first, re)) Contents.push_back(entry.first);
    }

    std::sort(Contents.begin(), Contents.end());
}
