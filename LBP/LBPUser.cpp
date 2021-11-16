#include <opencv2/opencv.hpp>
#include "lbp.hpp"
#include "LBPUser.hpp"
#include <string>
#include "opencv2/photo/photo.hpp"



cv::Mat lbpImage(cv::Mat& frame){

    // initial values
    int radius = 2;
    int neighbors = 8;

    cv::Mat dst; // image after preprocessing
    cv::Mat lbp; // lbp image


    int lbp_operator=2;

    //dst=frame;
    cv::cvtColor(frame, dst, cv::COLOR_BGR2GRAY);

    //Uncomment the next line to use smoothing
    //GaussianBlur(dst, dst, Size(7,7), 5, 3, BORDER_CONSTANT);

    switch(lbp_operator) {
    	case 0:
    		lbp::ELBP(dst, lbp, radius, neighbors); // use the extended operator
    		break;
    	case 1:
    		lbp::OLBP(dst, lbp); // use the original operator
    		break;
    	case 2:
    		lbp::VARLBP(dst, lbp, radius, neighbors);
    		break;
    	}
    // now to show the patterns a normalization is necessary
    // a simple min-max norm will do the job...
    cv::normalize(lbp, lbp, 0, 255, NORM_MINMAX, CV_8UC1);

    return lbp;
}


cv::Mat lbpImageSingleChan(cv::Mat& dst){

    // initial values
    int radius = 2;
    int neighbors = 8;

    //cv::Mat dst; // image after preprocessing
    cv::Mat lbp; // lbp image


    int lbp_operator=2;

    //dst=frame;
    //cv::cvtColor(frame, dst, CV_BGR2GRAY);

    //Uncomment the next line to use smoothing
    //GaussianBlur(dst, dst, Size(7,7), 5, 3, BORDER_CONSTANT);

    switch(lbp_operator) {
    	case 0:
    		lbp::ELBP(dst, lbp, radius, neighbors); // use the extended operator
    		break;
    	case 1:
    		lbp::OLBP(dst, lbp); // use the original operator
    		break;
    	case 2:
    		lbp::VARLBP(dst, lbp, radius, neighbors);
    		break;
    	}
    // now to show the patterns a normalization is necessary
    // a simple min-max norm will do the job...
    cv::normalize(lbp, lbp, 0, 255, NORM_MINMAX, CV_8UC1);

    return lbp;
}


cv::Mat lbpImageSingleChanExperimental(cv::Mat& dst){

    // initial values
    int radius = 4;
    int neighbors = 16;

    //cv::Mat dst; // image after preprocessing
    cv::Mat lbp; // lbp image


    int lbp_operator=2;

    //dst=frame;
    //cv::cvtColor(frame, dst, CV_BGR2GRAY);

    //Uncomment the next line to use smoothing
    //GaussianBlur(dst, dst, Size(7,7), 5, 3, BORDER_CONSTANT);

    switch(lbp_operator) {
    	case 0:
    		lbp::ELBP(dst, lbp, radius, neighbors); // use the extended operator
    		break;
    	case 1:
    		lbp::OLBP(dst, lbp); // use the original operator
    		break;
    	case 2:
    		lbp::VARLBP(dst, lbp, radius, neighbors);
    		break;
    	}
    // now to show the patterns a normalization is necessary
    // a simple min-max norm will do the job...
    cv::normalize(lbp, lbp, 0, 255, NORM_MINMAX, CV_8UC1);

    return lbp;
}
