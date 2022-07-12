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

#include "TMath.h"


AnalyzerUnit::AnalyzerUnit(std::string EventID, std::string ImageDir, int CameraNumber, Trainer** TrainedData, std::string MaskDir, Parser* Parser)
{
    /*Give the properties required to make the object - the identifiers i.e. the camera number, and location*/
    this->ImageDir=ImageDir;
    this->MaskDir=MaskDir;
    this->CameraNumber=CameraNumber;
    this->EventID=EventID;

    this->TrainedData = new Trainer(**TrainedData);
    this->MatTrigFrame = 0;
    this->loc_thres = 3;
    this->ratios.resize(256);
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


    cv::Mat workingFrame, diff_frame, subtr_frame, sigma_frame, subtr_avg_frame;
    cv::Mat prevFrame, prevDiff, prevPrevFrame;

    /*Static variable to store the threshold entropy WHERE USED??*/
    float entropyThreshold;
    //if(this->CameraNumber==2) entropyThreshold = 0.0009;
    //else entropyThreshold = 0.0003;
    entropyThreshold = 3.5;
    

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

    cv::Mat sqrt_trained_avg = this->TrainedData->TrainedAvgImage.clone();
    sqrt_mat(sqrt_trained_avg);
    
    if (startframe < 1) startframe = 1;
    
    if (startframe == 1){
        ratios.clear();
        ratios.resize(256);
        pix_counts.clear();
        pix_counts.resize(256);
    }
  
    cv::Mat comparisonFrame = cv::imread(refImg.c_str(),0);
    /*this->FileParser->GetImage(this->EventID, this->CameraFrames[startframe-2], prevFrame);
    this->FileParser->GetImage(this->EventID, this->CameraFrames[startframe-1], workingFrame);
    ProcessFrame(workingFrame,prevFrame,prevDiff);*/
    
    this->FileParser->GetImage(this->EventID, this->CameraFrames[startframe-1], prevFrame);
    prevPrevFrame = prevFrame;

    /* GaussianBlur can help with noisy images */
    //cv::GaussianBlur(comparisonFrame, comparisonFrame, cv::Size(5, 5), 0)

    /*Start by flagging that a bubble wasnt found, flag gets changed to 0 if all goes well*/
    this->TriggerFrameIdentificationStatus=-3;

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
        this->FileParser->GetImage(this->EventID, this->CameraFrames[i], workingFrame);

        /* GaussianBlur can help with noisy images */
        //cv::GaussianBlur(workingFrame, workingFrame, cv::Size(5, 5), 0)

        /*BackgroundSubtract*/
        ProcessFrame(workingFrame,prevPrevFrame,subtr_frame,5,nonStopMode ? -1 : i);    //Compare two frames back to get better sensitivity to slow growing bubbles

        /*Find LBP and then calculate Entropy*/
        //subtr_frame = lbpImage(diff_frame);
        
        //singleEntropy = this->calculateEntropyFrame(sigma_frame,!nonStopMode);
        singleEntropy = this->checkTriggerDerivative(subtr_frame, prevDiff,true,!nonStopMode);
        //singleEntropy = this->calculateSignificanceFrame(diff_frame,this->TrainedData->TrainedSigmaImage,!nonStopMode);
        /*
        ProcessFrame(workingFrame,TrainedData->TrainedAvgImage,subtr_avg_frame);
        std::cout << this->checkTriggerDerivative(subtr_avg_frame, prevDiff,false,!nonStopMode);
*/
        /* ****************
         * Debug Point here
         * **************** */
        if (!nonStopMode){
          //cv::Mat debug_mask;
          //cv::absdiff(workingFrame, /*TrainedData->TrainedAvgImage*/prevFrame, diff_frame);
          //cv::threshold(diff_frame, debug_mask, 6, 255, THRESH_BINARY); //Makes the image viewable in standard photo viewer without brightness adjustments
          std::cout<<"Entropy of BkgSub "<<i+30<<" image: "<<singleEntropy<<"\n";
          imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_"+this->CameraFrames[i], subtr_frame/*debug_mask*/);
          //std::cout << "diff_frame.at<float>(700,700): " << +(diff_frame.at<uchar>(700,700)) << std::endl;
          //std::cout << "workingFrame.at<float>(700,700): " << +(workingFrame.at<uchar>(700,700)) << std::endl;
          //this->calculateEntropyFrame(subtr_frame,true);
        }
        prevDiff = subtr_frame.clone();

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
                
                //Note: This block skips over anomalies in the image data and noise triggers.
                //      Set "numFramesCheck" to 0 if you want to trigger on these anomalies.
                int numFramesCheck = 2;
                cv::Mat tempPrevPrevFrame = prevFrame;
                cv::Mat tempPrevFrame = workingFrame;
                cv::Mat peakFrame;
                for (int ii = 1; ii <= numFramesCheck && ii+i < this->CameraFrames.size(); ii++){   //This means a trigger cannot occur after frame # 70-numFramesCheck
                
                    this->FileParser->GetImage(this->EventID, this->CameraFrames[i+ii], peakFrame);

                    //cv::absdiff(workingFrame, prevFrame, diff_frame);
                    ProcessFrame(peakFrame,tempPrevPrevFrame,diff_frame,5,nonStopMode ? -1 : i+ii);
                    if (!nonStopMode) imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_"+this->CameraFrames[i+ii], diff_frame);

                    //singleEntropy = this->calculateEntropyFrame(diff_frame);
                    singleEntropy = this->checkTriggerDerivative(diff_frame, prevDiff,false,!nonStopMode);
                    //singleEntropy = this->calculateSignificanceFrame(diff_frame,this->TrainedData->TrainedSigmaImage);
                    //std::cout << "singleEntropy: " << singleEntropy << "; 2/3 * entropyThreshold: " << 2/3 * entropyThreshold << std::endl;

                    if (singleEntropy <= /*2./3 **/ entropyThreshold) break;
                    else if (ii == numFramesCheck){
                        this->TriggerFrameIdentificationStatus = 0;
                        this->MatTrigFrame = i;
                    }
                    //else: check next frame
                    
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

void AnalyzerUnit::ProcessFrame(cv::Mat& workingFrame, cv::Mat& prevFrame, cv::Mat& diff_frame, int blur_diam, int img_num){
    cv::Mat pos_diff, neg_diff, blur_sigma;

    pos_diff = workingFrame - prevFrame - 6*this->TrainedData->TrainedSigmaImage;    //Note that negative numbers saturate to 0
    neg_diff = prevFrame - workingFrame - 6*this->TrainedData->TrainedSigmaImage;

    cv::GaussianBlur(pos_diff, pos_diff, cv::Size(blur_diam, blur_diam), 0);
    cv::GaussianBlur(neg_diff, neg_diff, cv::Size(blur_diam, blur_diam), 0);
    //cv::GaussianBlur(this->TrainedData->TrainedSigmaImage, blur_sigma, cv::Size(blur_diam, blur_diam), 0);
    /*
    pos_diff -= blur_sigma;
    neg_diff -= blur_sigma;
    *//*
    if (img_num >= 0){
        imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_pos_"+this->CameraFrames[img_num], pos_diff);
        imwrite(std::string(getenv("HOME"))+"/test/abub_debug/ev_"+EventID+"_neg_"+this->CameraFrames[img_num], neg_diff);
    }
    */
    cv::absdiff(pos_diff, neg_diff, diff_frame);

/*
    cv::absdiff(workingFrame, prevFrame, diff_frame);
    cv::GaussianBlur(diff_frame, diff_frame, cv::Size(5, 5), 0);
*/
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
            if (binEntry !=0){
                ImgEntropy -= binEntry * log2(binEntry);
                if (debug) std::cout << "i: " << i << "; binEntry * log2(binEntry) = " << binEntry << " * " << log2(binEntry) << " = " << binEntry * log2(binEntry) << std::endl;
            }
    }

    return ImgEntropy;
}

double AnalyzerUnit::checkTriggerDerivative(cv::Mat& ImageFrame, cv::Mat& LastFrame, bool store, bool debug){

    /*Memory for the image greyscale and histogram*/
    cv::Mat image_greyscale, image_greyscale_last, img_histogram, img_histogram_last, img_histogram_diff, img_histogram_sqrt;

    /*Histogram sizes and bins*/
    const int histSize[] = {256};
    float range[] = { 0, 256 };
    const float* histRange[] = { range };



    float ImgEntropy=0.0;
    /*Check if image is BW or colour*/
    if (ImageFrame.channels() > 1){
        /*Convert to BW*/
        cv::cvtColor(ImageFrame, image_greyscale, cv::COLOR_BGR2GRAY);
        cv::cvtColor(LastFrame, image_greyscale_last, cv::COLOR_BGR2GRAY);
    } else {
        /*The = operator assigns pointers so no memory is wasted*/
        image_greyscale = ImageFrame;
        image_greyscale_last = LastFrame;
    }


    /*Calculate Histogram*/
    cv::calcHist(&image_greyscale, 1, 0,        cv::Mat(), img_histogram, 1, histSize, histRange, true, false);
    cv::calcHist(&image_greyscale_last, 1, 0,        cv::Mat(), img_histogram_last, 1, histSize, histRange, true, false);
    /*cv::absdiff(img_histogram, img_histogram_last, img_histogram_diff);
    img_histogram_sqrt = img_histogram_diff.clone();
    sqrt_mat(img_histogram_sqrt);
    img_histogram_diff /= img_histogram_sqrt;*/
    /*Normalize Hist*/
    //img_histogram = img_histogram/(ImageFrame.rows*ImageFrame.cols);
    int temp = -1;
    /*Calculate Entropy*/
    float ratio_avg = -1;
    float last_ratio = -1;
    int pix_rem = ImageFrame.total();
    int pix_rem_last = LastFrame.total();
    //for (int i=img_histogram.rows-1; i>=0; i--){
    double significance = 0;
    double mean, sigma;
    int first_over_3p5 = loc_thres_max;
    int max_adc = 0;
    for (int i=0; i<img_histogram.rows; i++){
            float binEntry = img_histogram.at<float>(i, 0);
            float binEntry_prev = i ? img_histogram.at<float>(i-1, 0) : -1;
            float binEntry_prev_prev = i>1 ? img_histogram.at<float>(i-2, 0) : 0;
            float binEntry_last = img_histogram_last.at<float>(i, 0);
            if (pix_rem >0){
                //float ratio = i ? TMath::Max((binEntry - binEntry_prev_prev/100)/binEntry_prev,float(0.)) : -1;
                float ratio = i>1 ? binEntry/binEntry_prev : -1;
                if (ratio_avg < 0) ratio_avg = ratio;
                else ratio_avg = (ratio_avg + ratio)/2; //A moving average. Far away bins should become less and less significant.
                if (store){
                    ratios[i].push_back(ratio);
                    pix_counts[i].push_back(binEntry);
                }
                
                double bin_p_last = binEntry_last/pix_rem_last;
                double bin_p = binEntry/pix_rem;
                //stddev = sqrt(binEntry*(1-binEntry/pix_rem))/pix_rem;
                double stddev = sqrt(pix_rem*bin_p*(1-bin_p))/pix_rem;
                //stddev_last = sqrt(binEntry_last*(1-binEntry_last/pix_rem_last
                double stddev_last = sqrt(pix_rem_last*bin_p_last*(1-bin_p_last))/pix_rem_last;
                double sig = TMath::Abs(bin_p_last-bin_p)/sqrt(pow(stddev,2)+pow(stddev_last,2));
                
                if (debug){
                    std::cout << "binEntry " << i << ": " << binEntry << "; ";// << "; binEntry_last " << i << ": " << binEntry_last << "; ratio: " << ratio << std::endl;//"; last_ratio: " << last_ratio << std::endl;
                }
                
                if (i > 1){
                    //if ((last_ratio >= 0 && ratio > last_ratio*100) || (ratio > 0.1 && i>1)){
                    mean = CalcMean(pix_counts[i],pix_counts[0].size());
                    sigma = CalcStdDev(pix_counts[i],mean,pix_counts[0].size());
                    //std::cout << "sigma (before): " << sigma << "; pow(sigma,2): " << pow(sigma,2) << "; pow(pix_counts[0].size() - 5,2): " << pow(pix_counts[0].size() - 5,2) << "; ";
                    //std::cout << "pix_counts[0].size(): " << pix_counts[0].size() << "; pix_counts[0].size() - 5: " << pix_counts[0].size() - 5 << "; pow(pix_counts[0].size() - 5,2): " << pow(pix_counts[0].size() - 5,2) << "; ";
                    //if (pix_counts[0].size() < 5) sigma = sqrt(pow(sigma,2) + pow(5 - pix_counts[0].size(),2)); //Reduce the significance when low stats
                    //std::cout << "Add significance: " << (binEntry-mean)/sigma << "; mean: " << mean << "; sigma: " << sigma << std::endl;
                    if (binEntry > mean) significance += (binEntry-mean)/sigma;
                    if (debug) std::cout << "significance so far: " << significance << std::endl;
                    /*
                    if (binEntry > CalcMean(pix_counts[i]) && i>1){
                        if (ratio > 0.5) return true;   //Only ever gets this high if real bubble)
                        //Above noise?
                        for (int ii = 2; ii <= i; ii++){
                            mean = CalcMean(ratios[ii],ratios[0].size());
                            sigma = CalcStdDev(ratios[ii],mean,ratios[0].size());
                            if (debug) cout << "bin: " << ii << "; mean: " << mean << "; std_dev: " << sigma << std::endl;
                            if (ratio > mean+sigma*10){
                                if (debug) cout << "TRIGGER!" << std::endl;
                                return true;
                            }
                        }
                    }
                    */
                    //if (i <= loc_thres_max) this->loc_thres = i;    //If increasing loc_thres_max above 3, may have to tune the if statement here
                }
                
                if (significance > 3.5 && i < first_over_3p5) first_over_3p5 = i;
                
                if (i > max_adc) max_adc = i;
                
                if (store){
                    this->loc_thres = TMath::Max(first_over_3p5-1,max_adc-1);
                    if (this->loc_thres > loc_thres_max) this->loc_thres = loc_thres_max;
                }
                
                if (debug){
                    //std::cout << */"; ratio_avg: " << ratio_avg/* << "; sig: " << sig*/;
                    //std:: cout << "; bin_p_last: " << bin_p_last << "; bin_p: " << bin_p << "; stddev: " << stddev << "; stddev_last: " << stddev_last;
                    //std::cout << "; num: " << TMath::Abs(bin_p_last-bin_p) << "; den: " << sqrt(pow(stddev,2)+pow(stddev_last,2));
                    //std::cout << "; pix_rem: " << pix_rem << "; pix_rem_last: " << pix_rem_last;
                    //if (ratio_avg > 0.5 || sig > 2) std::cout << "\n\n\nTRIGGER\n\n\n";
                    //std::cout << "; Abs(binEntry-binEntry_last) " << i << ": " << TMath::Abs(binEntry-binEntry_last) << "; Abs(log10(binEntry)-log10(binEntry_last)) " << (binEntry_last ? TMath::Abs(std::log10(binEntry)-std::log10(binEntry_last)) : binEntry);
                    //std::cout << std::endl;
                    }
                    
                pix_rem -= binEntry;
                pix_rem_last -= binEntry_last;
                    
                if (temp<0) temp = i;
                last_ratio = ratio;
                //return i;
            }
    }
    
    //std::cout << "significance: " << significance << std::endl;

    return significance;
}

double AnalyzerUnit::calculateSignificanceFrame(cv::Mat& DiffFrame_raw, cv::Mat& TrainedSigma, bool debug){

    /*Memory for the image greyscale and histogram*/
    cv::Mat DiffFrame;
    
    /*Check if image is BW or colour*/
    if (DiffFrame_raw.channels() > 1){
        /*Convert to BW*/
        cv::cvtColor(DiffFrame_raw, DiffFrame, cv::COLOR_BGR2GRAY);
    } else {
        /*The = operator assigns pointers so no memory is wasted*/
        DiffFrame = DiffFrame_raw;
    }

    double Chi2=0.0;
    int NDF = DiffFrame.rows*DiffFrame.cols;
    
    for (int i = 0; i < DiffFrame.rows; i++){
      for (int j = 0; j < DiffFrame.cols; j++){
        if (TrainedSigma.at<uchar>(i,j) > 1e-5){  //Proxy for 0
          Chi2 += pow(DiffFrame.at<uchar>(i,j)/TrainedSigma.at<uchar>(i,j),2);
          if (debug && pow(DiffFrame.at<uchar>(i,j)*1,2)>100) std::cout << "Normal. DiffFrame.at<uchar>(" << i << "," << j << "): " << +(DiffFrame.at<uchar>(i,j)) << "; TrainedSigma.at<uchar>(" << i << "," << j << "): " << +(TrainedSigma.at<uchar>(i,j)) << std::endl;
        }
        else{
          Chi2 += pow(DiffFrame.at<uchar>(i,j)*1,2);  //Add 1 sigma significance for each intensity unit above 0 when noise level is 0
          if (debug && pow(DiffFrame.at<uchar>(i,j)*1,2)>100) std::cout << "No trained. DiffFrame.at<uchar>(" << i << "," << j << "): " << +(DiffFrame.at<uchar>(i,j)) << "; TrainedSigma.at<uchar>(" << i << "," << j << "): " << +(TrainedSigma.at<uchar>(i,j)) << std::endl;
        }
      }
    }

    if (debug) std::cout << "Chi2: " << Chi2 << "; NDF: " << NDF << std::endl;
    
    return TMath::Prob(Chi2,NDF);
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
