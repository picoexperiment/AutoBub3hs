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
#include "RawParser.hpp"
#include <dirent.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <string.h>
#include <sys/types.h> //.... added
#include <sys/stat.h>  //....

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#define debug false
#include <chrono>
using std::chrono::milliseconds;

RawParser::RawParser(std::string RunFolder, std::string ImageFolder, std::string ImageFormat) : Parser(RunFolder, ImageFolder, ImageFormat){}

RawParser* RawParser::clone(){
    return new RawParser(this->RunFolder, this->ImageFolder, this->ImageFormat);
}

int RawParser::GetImage(std::string EventID, std::string FrameName, cv::Mat &Image){
    auto t0 = std::chrono::high_resolution_clock::now();
    boost::filesystem::path imagePath(this->RunFolder);
    imagePath = imagePath / EventID / this->ImageFolder / FrameName;

    Image = cv::imread(imagePath.native(), 0);
    //std::cout << imagePath << " " << Image.empty() << std::endl;
    if (debug){
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> dt = t1 - t0;
        std::cout << "GetImage: " << dt.count() << std::endl;
    }
    return !Image.empty();
}

RawParser::~RawParser(){};

/*Function to Generate File Lists*/
void RawParser::GetFileLists(const char* EventFolder, std::vector<std::string>& FileList, const char* camera_out_name)
{
    DIR *dir  = opendir (EventFolder) ;
    if (dir)
    {
        /* print all the files and directories within directory */
        struct dirent* hFile;
        while (( hFile = readdir( dir )) != NULL )
        {
            if ( !strcmp( hFile->d_name, "."  )) continue;
            if ( !strcmp( hFile->d_name, ".." )) continue;

            // in linux hidden files all start with '.'
            if  ( hFile->d_name[0] == '.' ) continue;

            // dirFile.name is the name of the file. Do whatever string comparison
            // you want here. Something like:
            if ( strstr( hFile->d_name, camera_out_name ))
                //printf( "found an .bmp file: %s\n", hFile->d_name );
                FileList.push_back(std::string(hFile->d_name));
        }



        closedir (dir);
    }
    else
    {
        /* could not open directory */
        //perror ("");
        this->StatusCode = 1;
    }

}


/*Function to Generate File Lists*/
void RawParser::GetEventDirLists(std::vector<std::string>& EventList)
{
    DIR *dir  = opendir (this->RunFolder.c_str()) ;
    if (dir)
    {
        /* print all the files and directories within directory */
        struct dirent* hFile;
        while (( hFile = readdir( dir )) != NULL )
        {
            if ( !strcmp( hFile->d_name, "."  )) continue;
            if ( !strcmp( hFile->d_name, ".." )) continue;

            // in linux hidden files all start with '.'
            if  ( hFile->d_name[0] == '.' ) continue;

            // dirFile.name is the name of the file. Do whatever string comparison
            // you want here. Something like:
            //if ( hFile->d_type == DT_DIR )
            //    //printf( "found an .bmp file: %s\n", hFile->d_name );
            //    EventList.push_back(std::string(hFile->d_name)); // originally it was
            //
            // But we need the following to temporarily solve parsing issue
            char folder[200];
            strcpy(folder, this->RunFolder.c_str());
            char* currentPath = strcat(folder,hFile->d_name);
            struct stat statbuf;
            if(stat(currentPath, &statbuf) == -1)
            {
                perror("stat");
                exit(-1);
            }
            if(S_ISDIR(statbuf.st_mode))
            {
                EventList.push_back(std::string(hFile->d_name));
            }
        }



        closedir (dir);
    }
    else
    {
        /* could not open directory */
        //perror ("");
        this->StatusCode = 1;
    }

}

void RawParser::ParseAndSortFramesInFolder(std::string EventID, int Camera, std::vector<std::string>& Contents){
    using namespace boost::filesystem;

    path eventDir = this->RunFolder;
    eventDir /= EventID;
    eventDir /= this->ImageFolder;

    std::string re_str = "^.*cam" + std::to_string(Camera) + ".*(png|bmp)";
    boost::regex re(re_str);

    if (exists(eventDir) && is_directory(eventDir)){
        for (const directory_entry& x : directory_iterator(eventDir)){
            if (boost::regex_match(x.path().native(), re)){
                Contents.push_back(x.path().filename().native());
            }
        }
        std::sort(Contents.begin(), Contents.end());
    }
}


void RawParser::GetRunFileInfo(std::vector<std::string>& EventListFromFile){
    // Find the run number
    boost::filesystem::path runFolderPath = this->RunFolder;
    runFolderPath += "/";  // This ensures the last character is a slash, required for the next part
    std::string runID = runFolderPath.parent_path().filename().string();

    boost::filesystem::path runFilePath = (runFolderPath / runID).string() + ".txt";
    std::ifstream ifs(runFilePath.c_str());

    // Extract actual event numbers from run file
    std::string _;
    int eventNum;
    while (ifs >> _ >> eventNum >> _ >> _ >> _ >> _ >> _ >> _ >> _ >> _ >> _){
        EventListFromFile.push_back(std::to_string(eventNum));
    }
}
