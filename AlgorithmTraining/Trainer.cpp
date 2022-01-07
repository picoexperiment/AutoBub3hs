#include <vector>
#include <string>
#include <dirent.h>
#include <iostream>
#include <stdexcept>


#include <opencv2/opencv.hpp>
#include "Trainer.hpp"
#include "../LBP/LBPUser.hpp"
//#include "../ImageEntropyMethods/ImageEntropyMethods.hpp"
#include "../common/UtilityFunctions.hpp"
#include "../FrameSorter.hpp"


//#include "ImageEntropyMethods/ImageEntropyMethods.hpp"
//#include "LBP/LBPUser.hpp"

#define debug false



Trainer::Trainer(int camera,  std::vector<std::string> EventList, std::string EventDir, std::string ImageFormat, std::string ImageFolder)
{
    /*Give the properties required to make the object - the identifiers i.e. the camera number, and location*/
    this->camera=camera;
    this->EventList = EventList;
    this->EventDir = EventDir;
    this->ImageFormat = ImageFormat;
    this->ImageFolder = ImageFolder;

    // SearchPattern is used in sorting images in the image folder.
    std::string searchCode = "cam%d";
    int searchStart = ImageFormat.find(searchCode);
    this->SearchPattern = ImageFormat.substr(searchStart, searchStart + searchCode.size() + 5);

}

Trainer::Trainer(const Trainer &other_trainer){
    this->camera = other_trainer.camera;
    this->EventList = std::vector<std::string>(other_trainer.EventList);
    this->EventDir = std::string(other_trainer.EventDir);

    this->TrainedAvgImage = cv::Mat::zeros(other_trainer.TrainedAvgImage.rows, other_trainer.TrainedAvgImage.cols, other_trainer.TrainedAvgImage.type());
    other_trainer.TrainedAvgImage.copyTo(this->TrainedAvgImage);

    this->TrainedSigmaImage = cv::Mat::zeros(other_trainer.TrainedSigmaImage.rows, other_trainer.TrainedSigmaImage.cols, other_trainer.TrainedSigmaImage.type());
    other_trainer.TrainedSigmaImage.copyTo(this->TrainedSigmaImage);


    this->TrainedLBPAvg = cv::Mat::zeros(other_trainer.TrainedLBPAvg.rows, other_trainer.TrainedLBPAvg.cols, other_trainer.TrainedLBPAvg.type());
    other_trainer.TrainedLBPAvg.copyTo(this->TrainedLBPAvg);


    this->TrainedLBPSigma = cv::Mat::zeros(other_trainer.TrainedLBPSigma.rows, other_trainer.TrainedLBPSigma.cols, other_trainer.TrainedLBPSigma.type());
    other_trainer.TrainedLBPSigma.copyTo(this->TrainedLBPSigma);

    this->isLBPApplied = other_trainer.isLBPApplied;

    this->ImageFormat = other_trainer.ImageFormat;
    this->ImageFolder = other_trainer.ImageFolder;
    this->SearchPattern = other_trainer.SearchPattern;
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
            FrameSorter frameSorter(this->ImageFormat);
            std::sort(this->CameraFrames.begin(), this->CameraFrames.end(), frameSorter);//frameSortFuncTrainer);

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

void Trainer::CalculateMeanSigmaImageVector(std::vector<cv::Mat>& ImagesArray, cv::Mat& meanImageProcess, cv::Mat& sigmaImageProcess){

    int rows, cols;

    rows = ImagesArray[0].rows;
    cols = ImagesArray[0].cols;


    /*Declare new variables for making the analyzed matrix*/
    std::vector<cv::Mat>::size_type numImagesToProcess = ImagesArray.size();

    sigmaImageProcess = cv::Mat::zeros(rows, cols, CV_8U);
    meanImageProcess = cv::Mat::zeros(rows, cols, CV_8U);


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
                temp_pixVal = (float) ImagesArray[processingFrame].at<uchar>(row, col);

                /*online mean and variance algorithm*/
                temp_delta = temp_pixVal - temp_mean;
                temp_mean += temp_delta/(processingFrame+1);
                temp_m2 += temp_delta*(temp_pixVal - temp_mean);

            }

            temp_variance = temp_m2 / (numImagesToProcess - 1);
            temp_sigma = sqrt(temp_variance);

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

    //AvgImg = meanImageProcess.clone();
    //SigImg = sigmaImageProcess.clone();



}




void Trainer::MakeAvgSigmaImage(bool PerformLBPOnImages=false)
{

    this->isLBPApplied = PerformLBPOnImages;
    /*Declare memory to store all the images coming in*/
    std::vector<cv::Mat> backgroundImagingArray, backgroundLBPImgArray, TestingForEntropyArray;


    cv::Mat tempImagingProcess, tempImagingLBP, tempTestingEntropy;
    float singleEntropyTest=0;
    bool isThisAGoodEvent;



    printf("Camera %d training ... ",this->camera);
    /*Store all the images*/
    std::string ThisEventDir, thisEventLocation;


    for (int i=0; i<this->EventList.size(); i++)
    {
        ThisEventDir = this->EventDir + EventList[i] + this->ImageFolder;

        char tmpImageFilePattern[30];
        sprintf(tmpImageFilePattern, this->SearchPattern.c_str(), this->camera);
        std::string ImageFilePattern = tmpImageFilePattern;//"cam"+std::to_string(this->camera)+"_image";

        this->ParseAndSortFramesInFolder(ImageFilePattern, ThisEventDir);

        TestingForEntropyArray.clear();

        /*The for block loads images 0 and 1 from each event*/

        if (this->CameraFrames.size() >20 ){
            for (std::vector<int>::iterator it = TrainingSequence.begin(); it !=TrainingSequence.end(); it++){
                thisEventLocation = ThisEventDir + this->CameraFrames[*it];
                if (getFilesize(thisEventLocation) > 100000){ // it was 1000000=1MB, now 100kB=100000
                    tempImagingProcess = cv::imread(thisEventLocation, 0);

                    TestingForEntropyArray.push_back(tempImagingProcess);
                    isThisAGoodEvent = true;
                } else {
                    isThisAGoodEvent = false;
                    std::cout<<"Event "<<EventList[i]<<" is malformed. Skipping training on this event\n";
                    break;
                }
            }
        } else {
            std::cout<<"Event "<<EventList[i]<<" is nonexistant on the disk. Skipping training on this event\n";
            isThisAGoodEvent = false;
        }


        /*Test entropy for the first 2 trained sets*/
        if (isThisAGoodEvent){
            tempTestingEntropy = TestingForEntropyArray[1]-TestingForEntropyArray[0];
            singleEntropyTest = this->calculateEntropyFrame(tempTestingEntropy);
            //std::cout<<ThisEventDir<<ImageFilePattern<<" "<<singleEntropyTest<<"\n";
        } else {
            singleEntropyTest = 0.0000;
        }

        /*If entropy results pass, construct the images and LBP versions*/
        if (singleEntropyTest <= 0.0005 and isThisAGoodEvent) {
            advance_cursor();

            /*For all frames used for training*/
            for (cv::Mat image : TestingForEntropyArray) {
                backgroundImagingArray.push_back(image);

                /*LBP versions required?*/
                if (this->isLBPApplied) {
                    tempImagingLBP = lbpImageSingleChan(image);
                    backgroundLBPImgArray.push_back(tempImagingLBP);
                }
            }

        }

        /*GC*/
        this->CameraFrames.clear();
    }


    if (backgroundImagingArray.size()==0){
        printf("Training image set has 0 frames. This means that the event is malformed.\n");
        throw -7;
    }
    /*Calculate mean and sigma of the raw images*/
    advance_cursor();
    this->CalculateMeanSigmaImageVector(backgroundImagingArray, this->TrainedAvgImage, this->TrainedSigmaImage);
    if (debug){
        cv::imwrite("DebugPeek/AvgImage_cam" + std::to_string(this->camera) + ".png", this->TrainedAvgImage);
        cv::imwrite("DebugPeek/SigmaImage_cam" + std::to_string(this->camera) + ".png", this->TrainedSigmaImage);
    }

    /*Calculate mean and sigma of LBP*/
    advance_cursor();
    if (this->isLBPApplied) this->CalculateMeanSigmaImageVector(backgroundLBPImgArray, this->TrainedLBPAvg, this->TrainedLBPSigma);




    printf("complete.\n");

}


/*This function calculates ImageEntropy
based on CPU routines. It converts the image to
BW first. This is assumed that the image frames are
subtracted already
2021 11 19 - Colin M
    Moved here for implicit thread safety.
*/
float Trainer::calculateEntropyFrame(cv::Mat& ImageFrame){

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

bool Trainer::frameSortFuncTrainer(std::string i, std::string j)
{

    unsigned int sequence_i, camera_i;
    int got_i = sscanf(i.c_str(), this->ImageFormat.c_str(),
                       &camera_i, &sequence_i
                      );


    assert(got_i == 2);

    unsigned int  sequence_j, camera_j;
    int got_j = sscanf(j.c_str(),  this->ImageFormat.c_str(),
                       &camera_j, &sequence_j
                      );
    assert(got_j == 2);

    return sequence_i < sequence_j;

}
