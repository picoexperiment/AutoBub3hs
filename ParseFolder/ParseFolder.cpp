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
#include "ParseFolder.hpp"
#include <dirent.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#include <sys/types.h> //.... added
#include <sys/stat.h>  //....


/*Function to Generate File Lists*/
void GetFileLists(const char* EventFolder, std::vector<std::string>& FileList, int* statuscode, const char* camera_out_name)
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
        *statuscode = 1;
    }

}


/*Function to Generate File Lists*/
void GetEventDirLists(const char* RunFolder, std::vector<std::string>& EventList, int* statuscode)
{
    DIR *dir  = opendir (RunFolder) ;
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
            strcpy(folder,RunFolder);
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
        *statuscode = 1;
    }

}
