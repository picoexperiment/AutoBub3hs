/* Utility functions usually for debugging
 */



#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/features2d/features2d.hpp>



#include "UtilityFunctions.hpp"
#include <stdio.h>



void debugShow(cv::Mat& frame)
{

    cv::Mat cvFlippedImg;
    //cv::bitwise_not(frame, cvFlippedImg);
    cvFlippedImg = frame;

    const char* source_window = "DebugSource";
    cv::namedWindow( source_window, CV_WINDOW_NORMAL );

    cv::imshow(source_window, cvFlippedImg);
    cv::waitKey(0);

}

/*Workarounds for fermi grid old GCC*/
//bool bubbleBRectSort(cv::Rect a, cv::Rect b){return a.area()>b.area();}

/*Code to show histogram*/
void showHistogramImage(cv::Mat& frame)
{

    /* Establish the number of bins */
    int histSize = 256;

    /* Set the ranges ( for colour intensities) ) */
    float range[] = { 0, 256 } ;
    const float* histRange = { range };

    /*Extra Parameters*/
    bool uniform = true;
    bool accumulate = false;

    /*Calculation of the histogram*/
    cv::Mat frame_hist;
    calcHist( &frame, 1, 0, cv::Mat(), frame_hist, 1, &histSize, &histRange, uniform, accumulate );

    cv::Size s = frame_hist.size();
    int rows = s.height;
    int cols = s.width;

    for (int i=0; i<=256; i++){
        //std::cout<<"Rows: "<<rows<<" Cols: "<<cols<<"\n";
        std::cout<<i<<" "<<frame_hist.at<float>(i)<<"\n";
    }

}