#include <vector>
#include <string>
#include <dirent.h>
#include <iostream>
#include <functional>


#include <opencv2/opencv.hpp>


#include "AnalyzerUnit.hpp"
//#include "ImageEntropyMethods/ImageEntropyMethods.hpp"
#include "LBP/LBPUser.hpp"
#include "AlgorithmTraining/Trainer.hpp"
#include "common/UtilityFunctions.hpp"
#include "FrameSorter.hpp"


AnalyzerUnit::AnalyzerUnit(std::string EventID, std::string ImageDir, int CameraNumber, Trainer** TrainedData, std::string MaskDir, Parser* Parser)
{
    /*Give the properties required to make the object - the identifiers i.e. the camera number, and location*/
    this->ImageDir=ImageDir;
    this->MaskDir=MaskDir;
    this->CameraNumber=CameraNumber;
    this->EventID=EventID;

    this->TrainedData = new Trainer(**TrainedData);
    this->MatTrigFrame = 0;
    this->FileParser = Parser;

    this->FileParser->ParseAndSortFramesInFolder(this->EventID, this->CameraNumber, this->CameraFrames);
}

AnalyzerUnit::~AnalyzerUnit(void ){

    /*Clear the bubble memory pointers*/
    for (int i=0; i<this->BubbleList.size(); i++){
        delete this->BubbleList[i];
    }
    if (this->TrainedData) delete this->TrainedData;

    if (this->FileParser) delete this->FileParser;
}



/*! \brief Parse and sort a list of triggers from an event for a specific camera
 *
 * \param searchPattern
 * \return this->CameraFrames
 *
 * This function makes a list of all the file names matching the trigger
 * from a certain camera in a file and then makes aa sorted
 * list of all the images that can then be processed one by one .
 */


void AnalyzerUnit::ParseAndSortFramesInFolder( void )
{

    //std::string searchPattern = "cam"+std::to_string(this->CameraNumber)+"_image";

    char tmpSearchPattern[30];
    sprintf(tmpSearchPattern, TrainedData->SearchPattern.c_str(), this->CameraNumber);
    std::string searchPattern = tmpSearchPattern;

    /*Function to Generate File Lists*/
    {
        DIR *dir  = opendir (this->ImageDir.c_str()) ;
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
                if ( strstr( hFile->d_name, searchPattern.c_str() ))
                    //printf( "found an .bmp file: %s\n", hFile->d_name );
                    this->CameraFrames.push_back(std::string(hFile->d_name));
            }



            closedir (dir);
            FrameSorter frameSorter(this->TrainedData->ImageFormat);
            std::sort(this->CameraFrames.begin(), this->CameraFrames.end(), frameSorter);

        }
        else
        {
            /* could not open directory */
            //perror ("");
            this->StatusCode = 1;
        }


    }

}

/*! \brief Find the trigger frame
 *
 * \return int this->TriggerFrame
 *
 * Function uses the Image Entropy algorithm to search for
 * and decide the frame where the bubble can be first seen. (Trigger frame)
 */


void AnalyzerUnit::FindTriggerFrame(void ){

    /*First, check if the sequence of events is malformed*/
    if (this->CameraFrames.size()<5){
        this->okToProceed=false;
        this->TriggerFrameIdentificationStatus = -9;
        return;
    }


    cv::Mat workingFrame, img_mask0, img_mask1;
    cv::Mat prevFrame;

    /*Static variable to store the threshold entropy WHERE USED??*/
    float entropyThreshold;
    //if(this->CameraNumber==2) entropyThreshold = 0.0009;
    //else entropyThreshold = 0.0003;
    entropyThreshold = 0;
    

    /*Variable to store the current entropy in*/
    float singleEntropy;

    /*The reference image does not change, it is the first frame*/

    std::string refImg = this->ImageDir + this->CameraFrames[0];
    //std::cout<<"Ref Image: "<<refImg<<"\n";
    
    // REPLACE THIS CODE LATER - once Parser->GetImage has error codes
    /*
    if(getFilesize(refImg)<50000){
        std::cout << "Size of " << refImg << " is too small. Skipping this camera for this event." << std::endl;
        this->okToProceed=false;
        this->TriggerFrameIdentificationStatus = -9;
        return;
    }
    */

//    comparisonFrame = cv::imread(refImg.c_str());
    this->FileParser->GetImage(this->EventID, this->CameraFrames[0], prevFrame);

    /* GaussianBlur can help with noisy images */
    //cv::GaussianBlur(comparisonFrame, comparisonFrame, cv::Size(5, 5), 0)

    /*Start by flagging that a bubble wasnt found, flag gets changed to 0 if all goes well*/
    this->TriggerFrameIdentificationStatus=-3;

    for (int i = 1; i < this->CameraFrames.size(); i++) {
//    for (int i = 1; i < 30; i++) {

        /*The name and load to memory evalImage*/
        std::string evalImg = this->ImageDir + this->CameraFrames[i];

        /*Check if image is malformed. If yes, then stop*/
        // REPLACE THIS CODE LATER - once Parser->GetImage has error codes
        /*
        if(getFilesize(evalImg)<50000){
            std::cout << "Size of " << evalImg << " is too small. Skipping this camera for this event." << std::endl;
            this->okToProceed=false;
            this->TriggerFrameIdentificationStatus = -9;
            return;
        }*/
        //workingFrame = cv::imread(evalImg.c_str(), 0);
        this->FileParser->GetImage(this->EventID, this->CameraFrames[i], workingFrame);

        /* GaussianBlur can help with noisy images */
        //cv::GaussianBlur(workingFrame, workingFrame, cv::Size(5, 5), 0)

        /*BackgroundSubtract*/
        cv::absdiff(workingFrame, /*TrainedData->TrainedAvgImage*/prevFrame/*comparisonFrame*/, img_mask0);
        img_mask1 = img_mask0 - 6*this->TrainedData->TrainedSigmaImage;

        /*Find LBP and then calculate Entropy*/
        //img_mask1 = lbpImage(img_mask0);
        cv::Mat debug_mask;
        cv::threshold(img_mask0, debug_mask, 6, 255, THRESH_BINARY);
        singleEntropy = this->calculateEntropyFrame(img_mask1);

        /* ****************
         * Debug Point here
         * **************** */
        //std::cout<<"Entropy of BkgSub "<<i+30<<" image: "<<singleEntropy<<"\n";
//        debugShow(debug_mask);
//        imwrite("/home/carl/test/abub_debug/"+this->CameraFrames[i], debug_mask);


        /*Calculate entropy of ROI*/
        if (i==1 and singleEntropy>0.0005){
            //printf("************ ---> WARNING <--******************\n");
            //printf("Entropy is massive - something has triggered at the very first frame | ");
            //printf("Autobub Image analysis is meaningless on this data set. Manual check recommended\n");
            //printf(" *** Autobub skip --> *** \n");
            this->TriggerFrameIdentificationStatus = -2;
            this->okToProceed=false;
            break;
            //exit (EXIT_FAILURE);
        }
        //std::cout<<"Frame Entropy: "<<singleEntropy<<std::endl;

        /*Nothing works better than manual entropy settings. :-(*/
        if (singleEntropy > entropyThreshold and i > this->minEvalFrameNumber) {
            //std::cout << this->EventID << " " << this->CameraFrames[i] << " " << singleEntropy << std::endl;
            /* LED flicker check: check if the following frame is also
             * above the entropy threshold. Entropy for valid events grows,
             * while flicker events have one frame above threshold then goes
             * back below threshold.
             */
            if (i != this->CameraFrames.size()-1){
                std::string evalImg = this->ImageDir + this->CameraFrames[i+1];

                // REPLACE THIS CODE LATER - once Parser->GetImage has error codes
                /*
                if(getFilesize(evalImg)<50000){
                    this->okToProceed=false;
                    this->TriggerFrameIdentificationStatus = -9;
                    return;
                }
                */
                //workingFrame = cv::imread(evalImg.c_str(), 0);
                this->FileParser->GetImage(this->EventID, this->CameraFrames[i+1], workingFrame);

                cv::absdiff(workingFrame, prevFrame, img_mask0);

                singleEntropy = this->calculateEntropyFrame(img_mask0);

                if (singleEntropy > 2/3 * entropyThreshold){
                    this->TriggerFrameIdentificationStatus = 0;
                    this->MatTrigFrame = i;
                    break;
                }

            }

        }
        prevFrame = workingFrame;
    }


    if  (this->TriggerFrameIdentificationStatus==-3) {
        //printf("\n**No bubbles were found. Manually check this folder**\n");
        this->okToProceed=false;
        this->MatTrigFrame;
    }


}


/*This function calculates ImageEntropy
based on CPU routines. It converts the image to
BW first. This is assumed that the image frames are
subtracted already
2021 11 19 - Colin M
    Moved here for implicit thread safety.
*/
float AnalyzerUnit::calculateEntropyFrame(cv::Mat& ImageFrame){

    /*Memory for the image greyscale and histogram*/
    cv::Mat image_greyscale, img_histogram;

    /*Histogram sizes and bins*/
    const int histSize[] = {16};
    float range[] = { 0, 256 };
    const float* histRange[] = { range };



    float ImgEntropy=0.0;
    /*Check if image is BW or colour*/
    if (ImageFrame.channels() > 1){
        /*Convert to BW*/
        cv::cvtColor(ImageFrame, image_greyscale, cv::COLOR_BGR2GRAY);
    } else {
        /*The = operator assigns pointers so no memory is wasted*/
        image_greyscale = ImageFrame;
    }


    /*Calculate Histogram*/
    cv::calcHist(&image_greyscale, 1, 0,        cv::Mat(), img_histogram, 1, histSize, histRange, true, false);
    /*Normalize Hist*/
    img_histogram = img_histogram/(ImageFrame.rows*ImageFrame.cols);
    /*Calculate Entropy*/
    for (int i=0; i<img_histogram.rows; i++){
            float binEntry = img_histogram.at<float>(i, 0);
            if (binEntry !=0)
                ImgEntropy -= binEntry * log2(binEntry);
    }

    return ImgEntropy;
}

/*Misc functions*/

/*! \brief Sorting function for camera frames
 *
 * To be used by std::sort for sorting files associated with
 * the camera frames. Not to be used otherwise.
 */

bool frameSortFunc(std::string i, std::string j)
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
