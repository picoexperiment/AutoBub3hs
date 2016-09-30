#include <vector>
#include <string>
#include <dirent.h>
#include <iostream>


#include <opencv2/opencv.hpp>
#include "Trainer.hpp"
#include "../LBP/LBPUser.hpp"


//#include "ImageEntropyMethods/ImageEntropyMethods.hpp"
//#include "LBP/LBPUser.hpp"




Trainer::Trainer(int camera,  std::vector<std::string> EventList, std::string EventDir)
{
    /*Give the properties required to make the object - the identifiers i.e. the camera number, and location*/
    this->camera=camera;
    this->EventList = EventList;
    this->EventDir = EventDir;


}

Trainer::~Trainer(void ) {}



/*! \brief Parse and sort a list of triggers from an event for a specific camera
 *
 * \param searchPattern
 * \return this->CameraFrames
 *
 * This function makes a list of all the file names matching the trigger
 * from a certain camera in a file and then makes aa sorted
 * list of all the images that can then be processed one by one .
 */


void Trainer::ParseAndSortFramesInFolder(std::string searchPattern, std::string ImageDir )
{

    /*Function to Generate File Lists*/
    {
        DIR *dir  = opendir (ImageDir.c_str()) ;
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
                if ( strstr( hFile->d_name, searchPattern.c_str() )){
                    this->CameraFrames.push_back(std::string(hFile->d_name));
                    }
            }



            closedir (dir);

            std::sort(this->CameraFrames.begin(), this->CameraFrames.end(), frameSortFuncTrainer);

        }
        else
        {
            /* could not open directory */
            //perror ("");
            this->StatusCode = 1;
        }


    }

}

/*! \brief Make sigma and mean of training images
 *
 * \return cv::Mat this->TrainedAvgImage
 * \return cv::Mat this->TrainedSigmaImage
 *
 * Function finds avg and std of all training frames
 */


void Trainer::MakeAvgSigmaImage(bool PerformLBPOnImages=false)
{

    this->isLBPApplied = PerformLBPOnImages;
    /*Declare memory to store all the images coming in*/
    std::vector<cv::Mat> backgroundImagingArray;

    cv::Mat tempImagingProcess, tempImagingLBP;
    int rows, cols;

    /*Store all the images*/
    std::string ThisEventDir, thisEventLocation;

    for (int i=0; i<this->EventList.size(); i++)
    {
        ThisEventDir = this->EventDir+EventList[i]+"/Images/";
        printf("Now processing %s\n", ThisEventDir.c_str());

        std::string ImageFilePattern = "cam"+std::to_string(this->camera)+"_image";
        this->ParseAndSortFramesInFolder(ImageFilePattern, ThisEventDir);

        for (std::vector<int>::iterator it = TrainingSequence.begin(); it !=TrainingSequence.end(); it++)
        {
            thisEventLocation = ThisEventDir + this->CameraFrames[*it];
            //std::cout<<"Frame: "<<thisEventLocation<<"\n";
            tempImagingProcess = cv::imread(thisEventLocation, 0);

            if (PerformLBPOnImages){
                tempImagingLBP = lbpImageSingleChan(tempImagingProcess);
                backgroundImagingArray.push_back(tempImagingLBP);
            }
            else
            {
                backgroundImagingArray.push_back(tempImagingProcess);
            }
        }
        /*GC*/
        this->CameraFrames.clear();
    }



//    std::string ty =  type2str( backgroundImagingArray[0].type() );
//    printf("Matrix: %s %dx%d \n", ty.c_str(), backgroundImagingArray[0].cols, backgroundImagingArray[0].rows );

    rows = backgroundImagingArray[0].rows;
    cols = backgroundImagingArray[0].cols;

    //printf ("Rows %d cols %d, TotalImageSize: %d", rows, cols, backgroundImagingArray.size());


    /*Declare new variables for making the analyzed matrix*/
    std::vector<cv::Mat>::size_type numImagesToProcess = backgroundImagingArray.size();

    cv::Mat sigmaImageProcess = cv::Mat::zeros(rows, cols, CV_8U);
    cv::Mat meanImageProcess = cv::Mat::zeros(rows, cols, CV_8U);


    std::vector<cv::Mat>::size_type processingFrame;

    /*few temp variables*/
    float temp_variance=0;
    float temp_sigma=0;

    float temp_pixVal=0;
    float temp_mean=0;
    float temp_m2=0;
    float temp_delta=0;


    /*Make the sigma image*/
    for (int row=0; row<rows; row++)
    {
        for (int col=0; col<cols; col++)
        {
            /*get the 1-D array of each layer*/
            for (processingFrame=0; processingFrame<numImagesToProcess; processingFrame++ )
            {
                temp_pixVal = (float) backgroundImagingArray[processingFrame].at<uchar>(row, col);

                /*online mean and variance algorithm*/
                temp_delta = temp_pixVal - temp_mean;
                temp_mean += temp_delta/(processingFrame+1);
                temp_m2 += temp_delta*(temp_pixVal - temp_mean);

                //printf(" -%d-", (int)backgroundImagingArray[processingFrame].at<uchar>(row, col));
            }

            temp_variance = temp_m2 / (numImagesToProcess - 1);
            temp_sigma = sqrt(temp_variance);
            //printf("\n Mean: %f | Var %f | Sig %f \n", temp_mean, temp_variance, temp_sigma);

            /*Values in a CV_32F image must be between 0 and 1. Since the scale was 0-255 for pixels, we normalize it by dividing with 255*/
            sigmaImageProcess.at<uchar>(row, col) =  (int)temp_sigma;
            meanImageProcess.at<uchar>(row, col) =  temp_mean;



            /*clear memory for reuse in next iter*/
            temp_variance=0;
            temp_sigma=0;

            temp_pixVal=0;
            temp_mean=0;
            temp_m2=0;
            temp_delta=0;
        }
    }

    //sigmaImageProcess = sigmaImageProcess*100;
    //cv::Mat sigmaImageWrite, meanImageWrite;
    //sigmaImageProcess.convertTo(sigmaImageWrite, CV_8U, 255.0);
    //meanImageProcess.convertTo(meanImageWrite, CV_8U, 255.0);

    this->TrainedAvgImage = meanImageProcess;
    this->TrainedSigmaImage = sigmaImageProcess;
    //cv::imwrite( "SigmaMap.png", sigmaImageWrite );
    //cv::imwrite( "MeanMap.png", meanImageProcess );

}

/*Misc functions*/

/*! \brief Sorting function for camera frames
 *
 * To be used by std::sort for sorting files associated with
 * the camera frames. Not to be used otherwise.
 */

bool frameSortFuncTrainer(std::string i, std::string j)
{

    unsigned int sequence_i, camera_i;
    int got_i = sscanf(i.c_str(),  "cam%d_image%u.png",
                       &camera_i, &sequence_i
                      );


    assert(got_i == 2);

    unsigned int  sequence_j, camera_j;
    int got_j = sscanf(j.c_str(),  "cam%d_image%u.png",
                       &camera_j, &sequence_j
                      );
    assert(got_j == 2);

    return sequence_i < sequence_j;

}