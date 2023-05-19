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

    this->TrainedData = new Trainer(**TrainedData); //Uses copy constructor. Not sure why it's done this way...
    this->MatTrigFrame = 0;
    this->loc_thres = 3;
    this->pix_counts.resize(256);
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


void AnalyzerUnit::FindTriggerFrame(bool nonStopMode, int startframe){

    /*First, check if the sequence of events is malformed*/
    if (this->CameraFrames.size()<5){
        this->okToProceed=false;
        this->TriggerFrameIdentificationStatus = -9;
        return;
    }

    //assume there are just as many frames after frame 50 as before
    //this is only used for debugging (so far), so shouldn't matter for operation if it's not always correct
    int frame_num_offset = 50-(this->CameraFrames.size()-1)/2;

    cv::Mat workingFrame, subtr_frame, prevFrame, prevPrevFrame;

    /*Static variable to store the threshold entropy WHERE USED??*/
    float entropyThreshold;
    //if(this->CameraNumber==2) entropyThreshold = 0.0009;
    //else entropyThreshold = 0.0003;
    
    //PICO-40L thresholds
    //entropyThreshold = 8e-5;    //For actual entropy. Smallest to prevent noise triggers
    //entropyThreshold = 1.9; //For entropy significance. Largest to be able to get all the triggers. Get a lot of triggers on noise which get vetoed by frame ahead check
    entropyThreshold = 3.5; //For new significance. Ideal setting. Will hopefully not require tuning for future fills. Not yet tested on test chambers.
    

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
    
    if (startframe < 1) startframe = 1;
    
    if (startframe == 1){
        pix_counts.clear();
        pix_counts.resize(256);
        entropies.clear();
    }
    
    double gamma = 2.2;
  
    //cv::Mat comparisonFrame = cv::imread(refImg.c_str(),0);
    
    this->FileParser->GetImage(this->EventID, this->CameraFrames[startframe-1], prevFrame);gammaCorrection(prevFrame,prevFrame,gamma);
    if (startframe < 2) prevPrevFrame = prevFrame;
    else this->FileParser->GetImage(this->EventID, this->CameraFrames[startframe-2], prevPrevFrame);gammaCorrection(prevPrevFrame,prevPrevFrame,gamma);

    /*Start by flagging that a bubble wasnt found, flag gets changed to 0 if all goes well*/
    this->TriggerFrameIdentificationStatus=-3;
    
    bool twoFrameOffset = true;
    //Lose some sensitivity to slow bubbles if training sample size is too small.
    //Needed to deal with non-stochastic noise introduced by changes in local freon density.
    if (this->TrainedData->TrainingSetSize < 6){
        twoFrameOffset = false;
        entropyThreshold *= 5/3.5;
    }
    if (!nonStopMode) std::cout << "twoFrameOffset: " << twoFrameOffset << "; this->TrainedData->TrainingSetSize: " << this->TrainedData->TrainingSetSize << std::endl;

    for (int i = startframe; i < this->CameraFrames.size(); i++) {
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
        this->FileParser->GetImage(this->EventID, this->CameraFrames[i], workingFrame);gammaCorrection(workingFrame,workingFrame,gamma);

        /*Background Subtract*/
        
        ProcessFrame(workingFrame,twoFrameOffset?prevPrevFrame:prevFrame,subtr_frame,5,nonStopMode ? -1 : i);    //Compare two frames back to get better sensitivity to slow growing bubbles
        //ProcessFrame(workingFrame,prevFrame,subtr_frame,5,nonStopMode ? -1 : i);   //old way
        //Future task: Could do a comparison with both prev and prevPrev to get even better sensitivity!
        
        /*Find LBP and then calculate Entropy*/
        //subtr_frame = lbpImage(diff_frame);
        
        //Choose trigger algorithm (also need to update again below if changing this)
        //singleEntropy = this->calculateEntropyFrame(subtr_frame,!nonStopMode);                //Actual entropy
        //singleEntropy = this->calculateEntropySignificance(subtr_frame,true,!nonStopMode);    //Self-tuning entropy significance
        singleEntropy = this->calculateSignificanceFrame(subtr_frame,true,!nonStopMode);        //Triggers on changes in image histogram. Self-tuning.

        /* ****************
         * Debug Point here
         * **************** */
        if (!nonStopMode){
          //cv::Mat debug_mask;
          //cv::threshold(subtr_frame, debug_mask, 2, 255, THRESH_BINARY); //Makes the image viewable in standard photo viewer without brightness adjustments
          std::cout<<"Entropy of BkgSub "<<i+frame_num_offset<<" image: "<<singleEntropy<<"\n";
          imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_"+this->CameraFrames[i], subtr_frame/*debug_mask*/);
        }

        /*Calculate entropy of ROI*/
        /*if (i==1 and singleEntropy>0.0005){
            //printf("************ ---> WARNING <--******************\n");
            //printf("Entropy is massive - something has triggered at the very first frame | ");
            //printf("Autobub Image analysis is meaningless on this data set. Manual check recommended\n");
            //printf(" *** Autobub skip --> *** \n");
            this->TriggerFrameIdentificationStatus = -2;
            this->okToProceed=false;
            break;
            //exit (EXIT_FAILURE);
        }*/
        //std::cout<<"Frame Entropy: "<<singleEntropy<<std::endl;

        /*Nothing works better than manual entropy settings. :-(*/
        if (singleEntropy > entropyThreshold and i >= this->minEvalFrameNumber) {
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
                
                //Note: This block skips over anomalies in the image data and noise triggers.
                //      Set "numFramesCheck" to 0 if you want to trigger on these anomalies.
                int numFramesCheck = 2;
                cv::Mat tempPrevPrevFrame = prevFrame;
                cv::Mat tempPrevFrame = workingFrame;
                cv::Mat peakFrame, diff_frame;
                double max_so_far = singleEntropy;
                if (!nonStopMode) std::cout << "Checking whether trigger condition is met in next few frames." << std::endl;
                for (int ii = 1; ii <= numFramesCheck && ii+i < this->CameraFrames.size(); ii++){   //This means a trigger cannot occur after frame # 70-numFramesCheck
                
                    this->FileParser->GetImage(this->EventID, this->CameraFrames[i+ii], peakFrame);gammaCorrection(peakFrame,peakFrame,gamma);

                    ProcessFrame(peakFrame,twoFrameOffset?tempPrevPrevFrame:tempPrevFrame,diff_frame,5,nonStopMode ? -1 : i+ii);
                    if (!nonStopMode) imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_"+this->CameraFrames[i+ii], diff_frame);

                    //Trigger algorithm
                    //singleEntropy = this->calculateEntropyFrame(diff_frame,!nonStopMode);
                    //singleEntropy = this->calculateEntropySignificance(diff_frame,false,!nonStopMode);
                    singleEntropy = this->calculateSignificanceFrame(diff_frame,false,!nonStopMode);
                    
                    if (!nonStopMode) std::cout << "Frame ahead " << ii << " entropy: " << singleEntropy << std::endl;

                    if (singleEntropy/(entropyThreshold/3.5*5) + singleEntropy/max_so_far <= 3) break;  //Not a trigger
                    else if (ii == numFramesCheck){
                        this->TriggerFrameIdentificationStatus = 0;
                        this->MatTrigFrame = i;
                    }
                    //else: check next frame
                    if (singleEntropy > max_so_far) max_so_far = singleEntropy;
                    
                    tempPrevPrevFrame = tempPrevFrame;
                    tempPrevFrame = peakFrame;
                }
                if (this->TriggerFrameIdentificationStatus == 0) break;

            }

        }
        prevPrevFrame = prevFrame;
        prevFrame = workingFrame;
    }


    if  (this->TriggerFrameIdentificationStatus==-3) {
        //printf("\n**No bubbles were found. Manually check this folder**\n");
        this->okToProceed=false;
        this->MatTrigFrame;
    }


}

//From: https://lindevs.com/apply-gamma-correction-to-an-image-using-opencv/
void AnalyzerUnit::gammaCorrection(const cv::Mat &src, cv::Mat &dst, const float gamma)
{
    return; //DISABLED for now. Still need to test this.
    float invGamma = 1 / gamma;

    Mat table(1, 256, CV_8U);
    uchar *p = table.ptr();
    for (int i = 0; i < 256; ++i) {
        p[i] = (uchar) (pow(i / 255.0, invGamma) * 255);
    }

    LUT(src, table, dst);
}

void AnalyzerUnit::ProcessFrame(cv::Mat& workingFrame, cv::Mat& prevFrame, cv::Mat& diff_frame, int blur_diam, int img_num){
    cv::Rect ROI(0,0,workingFrame.cols,workingFrame.rows);
    ProcessFrame(workingFrame,prevFrame,diff_frame,blur_diam,ROI,img_num);
}

void AnalyzerUnit::ProcessFrame(cv::Mat& workingFrame, cv::Mat& prevFrame, cv::Mat& diff_frame, int blur_diam, cv::Rect ROI, int img_num){
    cv::Mat pos_diff, neg_diff, blur_sigma;
    
    diff_frame = cv::Mat::zeros(workingFrame.rows, workingFrame.cols, workingFrame.type());

    pos_diff = workingFrame(ROI) - prevFrame(ROI) - 6*this->TrainedData->TrainedSigmaImage(ROI);    //Note that negative numbers saturate to 0
    neg_diff = prevFrame(ROI) - workingFrame(ROI) - 6*this->TrainedData->TrainedSigmaImage(ROI);
    if (img_num >= 0){
        imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_pos_"+this->CameraFrames[img_num], pos_diff);
        imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_neg_"+this->CameraFrames[img_num], neg_diff);
    }

    /* GaussianBlur can help with noisy images */
    cv::GaussianBlur(pos_diff, pos_diff, cv::Size(blur_diam, blur_diam), 0);
    cv::GaussianBlur(neg_diff, neg_diff, cv::Size(blur_diam, blur_diam), 0);
    //cv::GaussianBlur(this->TrainedData->TrainedSigmaImage, blur_sigma, cv::Size(blur_diam, blur_diam), 0);
    if (img_num >= 0){
        imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_pos_filter_"+this->CameraFrames[img_num], pos_diff);
        imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_neg_filter_"+this->CameraFrames[img_num], neg_diff);
    }
    /*
    pos_diff -= blur_sigma;
    neg_diff -= blur_sigma;
    */
    cv::absdiff(pos_diff, neg_diff, diff_frame(ROI));

    /*
    cv::absdiff(workingFrame, prevFrame, diff_frame);  //Old method (just this line)
    diff_frame = diff_frame - 6*this->TrainedData->TrainedSigmaImage;*/
    //cv::GaussianBlur(diff_frame, diff_frame, cv::Size(5, 5), 0);

}

/*This function calculates ImageEntropy
based on CPU routines. It converts the image to
BW first. This is assumed that the image frames are
subtracted already
2021 11 19 - Colin M
    Moved here for implicit thread safety.
*/
float AnalyzerUnit::calculateEntropyFrame(cv::Mat& ImageFrame, bool debug){

    /*Memory for the image greyscale and histogram*/
    cv::Mat image_greyscale, img_histogram;

    /*Histogram sizes and bins*/
    const int histSize[] = {128};   //Number of bins. Tuned on PICO-40L first fill. PICO-60 used 16 bins.
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
            if (binEntry !=0){
                ImgEntropy -= binEntry * log2(binEntry);
                if (debug) std::cout << "i: " << i << "; binEntry * log2(binEntry) = " << binEntry << " * " << log2(binEntry) << " = " << binEntry * log2(binEntry) << std::endl;
            }
    }

    return ImgEntropy;
}

double AnalyzerUnit::calculateEntropySignificance(cv::Mat& ImageFrame, bool store, bool debug){
    double entropy = calculateEntropyFrame(ImageFrame,debug);
    if (store){
        entropies.push_back(entropy);
    }
    double mean = CalcMean(entropies);
    double sigma = CalcStdDev(entropies,mean);
    return (entropy-mean)/sigma;
}

double AnalyzerUnit::calculateSignificanceFrame(cv::Mat& ImageFrame, bool store, bool debug){

    /*Memory for the image greyscale and histogram*/
    cv::Mat image_greyscale, img_histogram;

    /*Histogram sizes and bins*/
    const int histSize[] = {256};
    float range[] = { 0, 256 };
    const float* histRange[] = { range };

    double significance = 0;
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
    /*Calculate Significance*/
    int pix_rem = ImageFrame.total();
    double mean, sigma;
    int first_over_3p5 = -1;
    int max_adc = 0;
    for (int i=0; i<img_histogram.rows; i++){
            float binEntry = img_histogram.at<float>(i, 0);
            if (pix_rem >0){
                if (store){
                    pix_counts[i].push_back(binEntry);
                }
                
                if (debug){
                    std::cout << "binEntry " << i << ": " << binEntry << "; ";
                }
                
                if (i > 1){
                    mean = CalcMean(pix_counts[i],pix_counts[0].size());
                    sigma = CalcStdDev(pix_counts[i],mean,pix_counts[0].size());
                    //if (pix_counts[0].size() < 5) sigma = sqrt(pow(sigma,2) + pow(5 - pix_counts[0].size(),2)); //Reduce the significance when low stats
                    //std::cout << "Add significance: " << (binEntry-mean)/sigma << "; mean: " << mean << "; sigma: " << sigma << std::endl;
                    if (binEntry != mean || sigma > 0) significance += (binEntry-mean)/sigma;   //If statement prevents nan. Using OR because the inf from divide by 0 is desired when not storing bin values
                    if (significance < 0) significance = 0;
                    if (debug) std::cout << "significance so far: " << significance << std::endl;
                }
                
                if (significance > 3.5 && first_over_3p5 < 0) first_over_3p5 = i;
                
                if (i > max_adc) max_adc = i;
                
                if (store){
                    this->loc_thres = std::max(first_over_3p5-1,max_adc-1);
                    if (this->loc_thres < 2) this->loc_thres = 2;
                    if (this->loc_thres > loc_thres_max || this->TrainedData->TrainingSetSize < 6) this->loc_thres = loc_thres_max;
                }
                    
                pix_rem -= binEntry;
            }
            else{
                if (debug && i==2) std::cout << std::endl;
                break;
            }
    }
    
    //std::cout << "significance: " << significance << std::endl;

    return significance;
}

/*Misc functions*/

/*! \brief Sorting function for camera frames
 *
 * To be used by std::sort for sorting files associated with
 * the camera frames. Not to be used otherwise.
 */

template <typename num>
double CalcMean(std::vector<num> &vec, int size){
    if (size == -1) size = vec.size();
    double sum = 0;
    for (num& val : vec){
        sum += val;
    }
    return sum/size;
}

template <typename num>
double CalcStdDev(std::vector<num> &vec, double mean, int size){
    if (size == -1) size = vec.size();
    double sum = 0;
    for (num& val : vec){
        sum += val*val;
    }
    return sqrt(sum/size - mean*mean);
}

void sqrt_mat(cv::Mat& M){
  for(int i = 0; i < M.rows; i++)
  {
      uchar* Mi = M.ptr<uchar>(i);
      for(int j = 0; j < M.cols; j++){
          Mi[j] = std::sqrt(Mi[j]) >= 1 ? std::sqrt(Mi[j]) : 1;
          //std::cout << +Mi[j] << std::endl;
      }
  }
      
}

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
