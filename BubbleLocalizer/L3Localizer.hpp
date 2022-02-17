/* *****************************************************************************
 * This file contains the necessary functions for ImageEntropy
 * ALGORITHM given an image, expect a number which is the ImageEntropy.
 *
 * by Pitam Mitra for the PICO Geyser ImageAnalysis Algorithm.
 *
 * Created: 30 Jan 2014
 *
 * Issues: We still dont know how to handle colour images with 3 channel_type entropy.
 * Sum them?
 *
 *******************************************************************************/


#ifndef ImageLocalization_Rough_HPP_INCLUDED
#define ImageLocalization_Rough_HPP_INCLUDED


/*Includes*/
#include <opencv2/opencv.hpp>
#include "../AnalyzerUnit.hpp"
#include "../AlgorithmTraining/Trainer.hpp"
#include <vector>
#include "../bubble/bubble.hpp"

/*Memory Allocations*/

class L3Localizer: public AnalyzerUnit{

    private:



        /*User Params*/
        bool nonStopMode;
        cv::Scalar color, color_orange, color_green, color_red;
        #define SEARCH_LEVEL_1 0
        #define SEARCH_LEVEL_2 1



        /*The top and bottom dark area cut*/
        int topCutCornerX;
        int topCutCornerY;


        /*Matrices to store the frames.*/
        cv::Mat presentationFrame;
        cv::Mat ComparisonFrame, triggerFrame;


        cv::Mat PostTrigWorkingFrame;



        /*Test functions and pass criteria*/
        void EllipseTest(cv::Mat&, cv::RotatedRect&, cv::Rect&, cv::Scalar&, std::vector<cv::RotatedRect>&,  int, bool drawEllipses=true);

        /*Fix dupes in search ROIs*/
        void reFixSearchROIs(std::vector<cv::RotatedRect>& );
        void rem_unique(std::vector<cv::Rect>&, std::vector<cv::Rect>& );




    public:
        /*Constructor and deconstructor*/
        L3Localizer(std::string, std::string, int, bool nonStopPref, Trainer**, std::string, Parser* );
        ~L3Localizer();

        /*Public functions exposing the interface*/
        //void LocalizeBottomBubble(cv::Mat&, cv::Mat&, std::vector<cv::RotatedRect>& );
        void CalculateInitialBubbleParams(void );
        void CalculateInitialBubbleParamsCam2(void );
        void CalculatePostTriggerFrameParams(int );
        void CalculatePostTriggerFrameParamsCam2(int );
        void printBubbleList(void);

        /*Public variables exposed for direct manipulation*/
        int numBubbleMultiplicity=0;
        bool Level1SuspicionFlag;

        void LocalizeOMatic(std::string);
        bool isInMask(cv::Rect*);


};

bool bubbleBRectSort(cv::RotatedRect , cv::RotatedRect );



#endif
