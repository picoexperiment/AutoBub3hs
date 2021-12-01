/* The objective of this localizer is to shrink the image down
 * by some method and then get a "rough idea" as to where the bubble should be
 *
 * Input: CV::mat with image
 * Output: ROI pairs, 2 of them - bubble and mirror
 *
 *
 * Why this method: Instead of a wild goose chase around the entire frame
 * which never worked (See testbed folder with all the "detectbub.cpp" files), I
 * am convinced that that is the wrong way to go and wont work. If we can hone in
 * on where the bubble is, we dont have to use aggressive filters and may have
 * a better chance on "seeing" the bubble.
 *
 * Note: I am trying to use the Google C++ Style Guide. Follow it please.
 */



#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/features2d/features2d.hpp>



#include "L3Localizer.hpp"
#include <stdio.h>


#include "../LBP/lbp.hpp"
#include "../LBP/LBPUser.hpp"
#include "../common/UtilityFunctions.hpp"
#include "../AnalyzerUnit.hpp"
#include "../AlgorithmTraining/Trainer.hpp"
#include "../bubble/bubble.hpp"
#include "../common/CommonParameters.h"

// Needed for opencv >= 3.0.0
#if CV_VERSION_MAJOR > 2
#include "opencv2/imgproc/types_c.h"
#endif


/* ******************************************************************************
 * This function is step 1 to the problem. The lower bubble is gonna be
 * much bigger than the upper one. This will allow us to get an initial guess
 * on the position of the bubble and then use aggressive techniques for the top bubble.
 *
 * Not going by the mistakes earlier, this will be OO
 * ******************************************************************************/


L3Localizer::L3Localizer(std::string EventID, std::string ImageDir, int CameraNumber, bool nonStopPref, Trainer** TrainedData):AnalyzerUnit(EventID, ImageDir, CameraNumber, TrainedData)
{


    /*User Prefs*/
    nonStopMode = nonStopPref;  /*Flag for non-stop operation vs debug*/
    color = cv::Scalar( 255, 255, 255); // White colour universal
    color_red = cv::Scalar( 0,0,255);  /*Red colour defined for drawing stuff*/
    color_orange = cv::Scalar( 0,140,255);  /*Orange colour defined for indicating level2 searc areaf*/
    color_green = cv::Scalar( 0, 255, 0);  /*Green colour defined for indicating Hough searc areaf*/

    /*set confidence flag to false at start*/
    Level1SuspicionFlag = false;

    /*The ROIs*/
    //topCutCornerX = 66;
    //topCutCornerY = 219;

    /*Info from fit*/
    numBubbleMultiplicity = 0;


}


L3Localizer::~L3Localizer() {

        std::cout<<"Releasing memory\n";
        this->presentationFrame.release();
        this->ComparisonFrame.release();
        this->triggerFrame.release();

        //this->PostTrigWorkingFrame.refcount=0;
        this->PostTrigWorkingFrame.release();


}






/*This is the infamous ellipse - to - box area and eccentricity test to check whether the detection is a bubble or garbage.
 *Essentially what happens is that the ellipse should:
 *1. Not have an area less than 100 sq px
 *2. Not have an area bigger than the bounding box
 *3. Should not have a crazy eccentricity. The large bubbles are fairly roundish, the garbage is severely elongated
 */
void L3Localizer::EllipseTest(cv::Mat& frameDraw, cv::RotatedRect& minEllipse, cv::Rect& boundingBoxRect, cv::Scalar& color, std::vector<cv::RotatedRect>& bubbleLocations, int localizationDict, bool drawEllipses)
{

    float w,h, ellipseArea, boxArea;
    w = minEllipse.size.width;
    h = minEllipse.size.height;
    //minEllipse.center.x+=this->topCutCornerX;
    //minEllipse.center.y+=this->topCutCornerY;

    /*Area of Ellipse*/
    ellipseArea = 3.14159*w*h/4;
    boxArea = boundingBoxRect.width*boundingBoxRect.height;

    /*Debug Info
    std::cout<<"Rect Area: "<<boundingBoxRect.area()<<std::endl;
    */


    if (w/h >0.33 and w/h<3 and (ellipseArea > 0.5*boxArea and ellipseArea < 1.1*boxArea))
    {


        if (drawEllipses) cv::ellipse( frameDraw, minEllipse, color, 2, 8 );

        bubbleLocations.push_back(minEllipse);

        if (localizationDict== SEARCH_LEVEL_1)
        {



            /*Most bubbles shouldbe between 200-500. If not, start the Level 2 search*/
            ellipseArea >= 5000.0 ? this->Level1SuspicionFlag=true : this->Level1SuspicionFlag=false;
            this->numBubbleMultiplicity++;
        }


    }

}


/* Function to check for duplicates and overlap on ROI
 * It destroys the original vector but a new one
  * is made to take its place*/

void L3Localizer::rem_unique(std::vector<cv::Rect>& L2SearchAreas, std::vector<cv::Rect>& L2SearchAreasFixed)
{

    cv::Rect checkElem = L2SearchAreas[0];//.boundingRect();
    int cOrigHeight = checkElem.height;
    //checkElem.height += checkElem.height*1.0;

    if (L2SearchAreas.size()==1)
    {
        checkElem.height += cOrigHeight*1.0;
        L2SearchAreasFixed.push_back(checkElem);
        L2SearchAreas.erase(L2SearchAreas.begin());
        return;

    }
    bool foundOvrLap = false;
    for(std::vector<int>::size_type rects=1; rects<L2SearchAreas.size(); rects++)
    {
        cv::Rect thisElem = L2SearchAreas[rects];//.boundingRect();

        /*Backwards move*/
        checkElem.y -= cOrigHeight;
        /*Forward check*/
        checkElem.height += 2.0*cOrigHeight;


        cv::Rect OverLap = thisElem & checkElem;
        bool isOverlap = OverLap.area()!=0;


        if (isOverlap)
        {
            thisElem.height += thisElem.height*0.5;
            L2SearchAreasFixed.push_back(thisElem | checkElem);
            L2SearchAreas.erase(L2SearchAreas.begin()+rects);
            foundOvrLap = true;
        }

        /*Revert back*/
        checkElem.height -= 2.0*cOrigHeight;
        /*Backwards check*/
        checkElem.y += cOrigHeight*1.0;


    }
    /*Give a boost to height*/
    checkElem.height += cOrigHeight*1.0;
    if (!foundOvrLap) L2SearchAreasFixed.push_back(checkElem);
    L2SearchAreas.erase(L2SearchAreas.begin());

}



/* ******************************************************************************
 * This function is the primary localizer for the bubble finding algorithm.
 * However, it has no idea how big the bubble is and where to look for.
 * So the initial guess is the adaptive filtering.
 *
 * Why this strange name? It is a memory from the original CGeyserImageAnalysis code
 * written for the geyser, but now for COUPP-60.
 *
 * This routine is designed to look at the next frame and perform a guided search of the first frame
 * In case the bubbles are too small, this considerably narrows down the search areas
 * that the code has to look at. So this should make it more accurate
 *
 *  * *****************************************************************************/

void L3Localizer::CalculateInitialBubbleParams(void )
{




    /*Construct the frame differences and LBPImage Frames*/
    cv::Mat NewFrameDiffTrig, overTheSigma;
    cv::absdiff(this->triggerFrame, this->TrainedData->TrainedAvgImage, NewFrameDiffTrig);

    /*Debug*/
    if (!this->nonStopMode) cv::imwrite("DebugPeek/"+std::to_string(CameraNumber)+"_1_TrigTrainAbsDiff.png", NewFrameDiffTrig);

    overTheSigma = NewFrameDiffTrig - 6*this->TrainedData->TrainedSigmaImage;

    /*Debug*/
    if (!this->nonStopMode) cv::imwrite("DebugPeek/"+std::to_string(CameraNumber)+"_2_OvrThe6Sigma.png", overTheSigma);


    cv::blur(overTheSigma,overTheSigma, cv::Size(3,3));

    cv::threshold(overTheSigma, overTheSigma, 3, 255, CV_THRESH_TOZERO);
    cv::threshold(overTheSigma, overTheSigma, 0, 255, CV_THRESH_BINARY|CV_THRESH_OTSU);

    /*Debug*/
    if (!this->nonStopMode) cv::imwrite("DebugPeek/"+std::to_string(CameraNumber)+"_3_OtsuThresholded.png", overTheSigma);




    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(overTheSigma, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1);

    /*Make two vectors to store the fitted rectanglse and ellipses*/
    //std::vector<cv::RotatedRect> minAreaRect( contours.size() );
    std::vector<cv::Rect> minRect( contours.size() );

    int BoxArea=0;
    /*Generate the ellipses and rectangles for each contours*/
    for( int i = 0; i < contours.size(); i++ ) {
        minRect[i] = cv::boundingRect( contours[i]);
        //minAreaRect[i] = cv::minAreaRect(contours[i] );

        BoxArea = minRect[i].width*minRect[i].height;
        if (BoxArea>10){
            //std::cout<<" Bubble genesis              X: "<<minRect[i].x<<" Y: "<<minRect[i].y<<" W: "<<minRect[i].width<<" H: "<<minRect[i].height<<"\n";
            cv::rectangle(this->presentationFrame, minRect[i], this->color_red,1,8,0);
            this->bubbleRects.push_back(minRect[i]);

            BubbleImageFrame _thisBubbleFrame;
            _thisBubbleFrame.ContArea = cv::contourArea(contours[i]);
            _thisBubbleFrame.ContRadius = sqrt(_thisBubbleFrame.ContArea/3.14159);
            _thisBubbleFrame.newPosition = minRect[i];
            _thisBubbleFrame.moments = cv::moments(contours[i], false); /*second parameter is for a binary image*/
            _thisBubbleFrame.MassCentres = cv::Point2f( _thisBubbleFrame.moments.m10/_thisBubbleFrame.moments.m00 ,
                                                        _thisBubbleFrame.moments.m01/_thisBubbleFrame.moments.m00);

            //bubble* firstBubble = new bubble(minAreaRect[i]);
            bubble* firstBubble = new bubble(_thisBubbleFrame);
            this->BubbleList.push_back(firstBubble);

        }

    }

    /*Debug*/
    if (!this->nonStopMode) cv::imwrite("DebugPeek/"+std::to_string(CameraNumber)+"_4_BubbleDetected.png", this->presentationFrame);


    //NewFrameDiffTrig.refcount=0;
    //overTheSigma.refcount=0;
    NewFrameDiffTrig.release();
    overTheSigma.release();
    //debugShow(this->presentationFrame);

}



void L3Localizer::CalculateInitialBubbleParamsCam2(void )
{

    /*Temporary holder for the presentationFrame*/
    //cv::Mat tempPresentation;
    //tempPresentation = this->presentationFrame.clone();



    /*Construct the frame differences. Note: Due to retroreflector, the bubbles are darker!*/
    cv::Mat NewFrameDiffTrig, overTheSigma;
    NewFrameDiffTrig =  this->TrainedData->TrainedAvgImage-this->triggerFrame;

    /*Blur the trained data sigma image to get a bigger coverage and subtract*/
    cv::blur(this->TrainedData->TrainedSigmaImage, this->TrainedData->TrainedSigmaImage, cv::Size(3,3));
    overTheSigma = NewFrameDiffTrig - 6*this->TrainedData->TrainedSigmaImage;
    /*Enhance the difference*/
    overTheSigma*=5;


    /*Shadow removal using blurring and intensity*/
    cv::Mat bubMinusShadow;
    cv::blur(overTheSigma,overTheSigma, cv::Size(3,3));
    cv::threshold(overTheSigma, bubMinusShadow, 100, 255, CV_THRESH_TOZERO|CV_THRESH_OTSU);


    //debugShow(bubMinusShadow);

    /*Get rid of pixel noise*/
    cv::threshold(bubMinusShadow, bubMinusShadow, 10, 255, CV_THRESH_TOZERO);

    //debugShow(bubMinusShadow);
    /*Check if this is a trigger by the interface moving or not - Note: Works ONLY on cam 2's entropy settings*/
    //showHistogramImage(bubMinusShadow);
    float ImageDynamicRange = ImageDynamicRangeSum(bubMinusShadow,60,200);
    if (ImageDynamicRange==0.0) return;


    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(bubMinusShadow, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1);

    /*Make two vectors to store the fitted rectanglse and ellipses*/
    //std::vector<cv::RotatedRect> minAreaRect( contours.size() );
    std::vector<cv::Rect> minRect( contours.size() );



    int BoxArea=0;
    /*Generate the ellipses and rectangles for each contours*/
    for( int i = 0; i < contours.size(); i++ ) {
        minRect[i] = cv::boundingRect( contours[i]);
        //minAreaRect[i] = cv::minAreaRect(contours[i]);
        BoxArea = minRect[i].width*minRect[i].height;

        if (BoxArea>10){
            //std::cout<<" Bubble genesis              X: "<<minRect[i].x<<" Y: "<<minRect[i].y<<" W: "<<minRect[i].width<<" H: "<<minRect[i].height<<"\n";
            cv::rectangle(this->presentationFrame, minRect[i], this->color_red,1,8,0);
            this->bubbleRects.push_back(minRect[i]);

            BubbleImageFrame _thisBubbleFrame;
            _thisBubbleFrame.ContArea = cv::contourArea(contours[i]);
            _thisBubbleFrame.ContRadius = sqrt(_thisBubbleFrame.ContArea/3.14159);
            _thisBubbleFrame.newPosition = minRect[i];
            _thisBubbleFrame.moments = cv::moments(contours[i], false); /*second parameter is for a binary image*/
            _thisBubbleFrame.MassCentres = cv::Point2f( _thisBubbleFrame.moments.m10/_thisBubbleFrame.moments.m00 ,
                                                        _thisBubbleFrame.moments.m01/_thisBubbleFrame.moments.m00);



            //bubble* firstBubble = new bubble(minAreaRect[i]);
            bubble* firstBubble = new bubble(_thisBubbleFrame);
            this->BubbleList.push_back(firstBubble);

        }

    }

    //NewFrameDiffTrig.refcount=0;
    //overTheSigma.refcount=0;
    NewFrameDiffTrig.release();
    overTheSigma.release();

    //debugShow(this->presentationFrame);


}

/*Post trigger frame for cam 2*/

void L3Localizer::CalculatePostTriggerFrameParamsCam2(int postTrigFrameNumber )
{

    //cv::Mat tempPresentation;
    /*Declare memory / variables that will be needed */
    this->PostTrigWorkingFrame = cv::Mat();
    this->PostTrigWorkingFrame = cv::imread(this->ImageDir + this->CameraFrames[this->MatTrigFrame+1+postTrigFrameNumber],0);
    //tempPresentation =  this->PostTrigWorkingFrame.clone();
    //cv::cvtColor(tempPresentation, tempPresentation, cv::COLOR_GRAY2BGR);

    /*Construct the frame differences. Note: Due to retroreflector, the bubbles are darker!*/
    cv::Mat NewFrameDiffTrig, overTheSigma, newFrameTrig;
    NewFrameDiffTrig =  this->TrainedData->TrainedAvgImage-this->PostTrigWorkingFrame;

    /*Blur the trained data sigma image to get a bigger coverage and subtract*/
    cv::blur(this->TrainedData->TrainedSigmaImage, this->TrainedData->TrainedSigmaImage, cv::Size(3,3));
    overTheSigma = NewFrameDiffTrig - 6*this->TrainedData->TrainedSigmaImage;
    /*Enhance the difference*/
    overTheSigma*=5;


    /*Shadow removal using blurring and intensity*/
    cv::Mat bubMinusShadow;
    cv::blur(overTheSigma,overTheSigma, cv::Size(3,3));
    cv::threshold(overTheSigma, bubMinusShadow, 100, 255, CV_THRESH_TOZERO|CV_THRESH_OTSU);




    /*Get rid of pixel noise*/
    cv::threshold(bubMinusShadow, bubMinusShadow, 10, 255, CV_THRESH_TOZERO);

    /*Check if this is a trigger by the interface moving or not - Note: Works ONLY on cam 2's entropy settings*/
    //showHistogramImage(bubMinusShadow);
    float ImageDynamicRange = ImageDynamicRangeSum(bubMinusShadow,100,200);
    if (ImageDynamicRange==0.0) return;


    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(bubMinusShadow, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1);

    /*Make two vectors to store the fitted rectanglse and ellipses*/
    //std::vector<cv::RotatedRect> minAreaRect( contours.size() );
    std::vector<cv::Rect> minRect( contours.size() );
    std::vector<cv::Rect> newPositions;
    std::vector<double> ContArea;
    std::vector<cv::Moments> moments;
    std::vector<cv::Point2f> MassCentres;

    std::vector<BubbleImageFrame> BubbleTrackingPerFrames;




    int BoxArea=0;
    /*Generate the ellipses and rectangles for each contours*/
    for( int i = 0; i < contours.size(); i++ ) {
        minRect[i] = cv::boundingRect( contours[i]);
        //minAreaRect[i] = cv::minAreaRect(contours[i]);


        BoxArea = minRect[i].width*minRect[i].height;
        if (BoxArea>10){
            //bubble* firstBubble = new bubble(minRect[i]);
            //cv::rectangle(tempPresentation, minRect[i], this->color_red,1,8,0);
            newPositions.push_back(minRect[i]);
            BubbleImageFrame _thisBubbleFrame;
            _thisBubbleFrame.ContArea = cv::contourArea(contours[i]);
            _thisBubbleFrame.ContRadius = sqrt(_thisBubbleFrame.ContArea/3.14159);
            _thisBubbleFrame.newPosition = minRect[i];
            _thisBubbleFrame.moments = cv::moments(contours[i], false); /*second parameter is for a binary image*/
            _thisBubbleFrame.MassCentres = cv::Point2f( _thisBubbleFrame.moments.m10/_thisBubbleFrame.moments.m00 ,
                                                        _thisBubbleFrame.moments.m01/_thisBubbleFrame.moments.m00);

            BubbleTrackingPerFrames.push_back(_thisBubbleFrame);


        }
    }


    /*UnLock the bubble descriptors*/
    for (int a=0; a<this->BubbleList.size(); a++){
        this->BubbleList[a]->lockThisIteration=false;
    }

    /*Match these with the global bubbles*/
    for (int j=0; j<BubbleTrackingPerFrames.size(); j++){
        float _thisbubbleX=BubbleTrackingPerFrames[j].MassCentres.x;
        float _thisbubbleY=BubbleTrackingPerFrames[j].MassCentres.y;
        /*look through all the global bubbles for a position*/
        for (int k=0; k<this->BubbleList.size(); k++){
            float _eval_bubble_X=this->BubbleList[k]->last_x;
            float _eval_bubble_Y=this->BubbleList[k]->last_y;

            if ((_eval_bubble_X-_thisbubbleX<5) && (fabs(_eval_bubble_Y-_thisbubbleY)<4)){
                    *this->BubbleList[k]<<BubbleTrackingPerFrames[j];

                    break;
            }
        }

    }


    //this->PostTrigWorkingFrame.refcount=0;
    //this->PostTrigWorkingFrame.release();
    //tempPresentation.release();

}


/*For other cameras*/


void L3Localizer::CalculatePostTriggerFrameParams(int postTrigFrameNumber){

    //cv::Mat tempPresentation;

    /*Load the post trig frame*/
    this->PostTrigWorkingFrame = cv::imread(this->ImageDir + this->CameraFrames[this->MatTrigFrame+1+postTrigFrameNumber],0);
    //tempPresentation =  this->PostTrigWorkingFrame.clone();
    //cv::cvtColor(tempPresentation, tempPresentation, cv::COLOR_GRAY2BGR);




    /*Construct the frame differences*/
    cv::Mat NewFrameDiffTrig, overTheSigma, LBPImageTrigBeforeBlur, LBPImageTrigAfterBlur;
    cv::absdiff(this->PostTrigWorkingFrame, this->TrainedData->TrainedAvgImage, NewFrameDiffTrig);

    /*Calculate pixels over the sigma*/
    overTheSigma = NewFrameDiffTrig - 6*this->TrainedData->TrainedSigmaImage;

    /*Blur and threshold to remove pixel noise*/
    cv::blur(overTheSigma,overTheSigma, cv::Size(3,3));
    cv::threshold(overTheSigma, overTheSigma, 3, 255, CV_THRESH_TOZERO);
    cv::threshold(overTheSigma, overTheSigma, 0, 255, CV_THRESH_BINARY|CV_THRESH_OTSU);



    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(overTheSigma, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_TC89_L1);

    /*Make two vectors to store the fitted rectanglse and ellipses*/
    //std::vector<cv::RotatedRect> minAreaRect( contours.size() );
    std::vector<cv::Rect> minRect( contours.size() );
    std::vector<cv::Rect> newPositions;


    std::vector<BubbleImageFrame> BubbleTrackingPerFrames;



    int BoxArea=0;
    /*Generate the ellipses and rectangles for each contours*/
    for( int i = 0; i < contours.size(); i++ ) {
        minRect[i] = cv::boundingRect( contours[i]);
        //minAreaRect[i] = cv::minAreaRect( contours[i] );

        BoxArea = minRect[i].width*minRect[i].height;
        if (BoxArea>10){
            //std::cout<<" Bubble progression step:"<<postTrigFrameNumber<<" | X: "<<minRect[i].x<<" Y: "<<minRect[i].y<<" W: "<<minRect[i].width<<" H: "<<minRect[i].height<<"\n";
            //cv::rectangle(tempPresentation, minRect[i], this->color_red,1,8,0);
            newPositions.push_back(minRect[i]);

            BubbleImageFrame _thisBubbleFrame;
            _thisBubbleFrame.ContArea = cv::contourArea(contours[i]);
            _thisBubbleFrame.ContRadius = sqrt(_thisBubbleFrame.ContArea/3.14159);
            _thisBubbleFrame.newPosition = minRect[i];
            _thisBubbleFrame.moments = cv::moments(contours[i], false); /*second parameter is for a binary image*/
            _thisBubbleFrame.MassCentres = cv::Point2f( _thisBubbleFrame.moments.m10/_thisBubbleFrame.moments.m00 ,
                                                        _thisBubbleFrame.moments.m01/_thisBubbleFrame.moments.m00);

            BubbleTrackingPerFrames.push_back(_thisBubbleFrame);

            //this->bubbleRects.push_back(minRect[i]);
        }

    }

    //std::cout<<"Sizes, newPositions: "<<newPositions.size()<<" NewPos RR: "<<newPositionsRotatedRect.size()<<"\n";


    /*UnLock the bubble descriptors*/
    for (int a=0; a<this->BubbleList.size(); a++){
        this->BubbleList[a]->lockThisIteration=false;
    }

    /*Match these with the global bubbles*/
    for (int j=0; j<BubbleTrackingPerFrames.size(); j++){
        float _thisbubbleX=BubbleTrackingPerFrames[j].MassCentres.x;
        float _thisbubbleY=BubbleTrackingPerFrames[j].MassCentres.y;
        /*look through all the global bubbles for a position*/
        for (int k=0; k<this->BubbleList.size(); k++){
            float _eval_bubble_X=this->BubbleList[k]->last_x;
            float _eval_bubble_Y=this->BubbleList[k]->last_y;

            if ((_eval_bubble_X-_thisbubbleX<5) && (fabs(_eval_bubble_Y-_thisbubbleY)<5)){

                    //std::cout<<"Bubble adding with RR cen X"<<newPositionsRotatedRect[j].center.x<<" y "<<newPositionsRotatedRect[j].center.y<<"\n";
                    *this->BubbleList[k]<<BubbleTrackingPerFrames[j];
                    break;
            }
        }

    }

    //this->PostTrigWorkingFrame.refcount=0;
    //this->PostTrigWorkingFrame.release();

    //debugShow(tempPresentation);
}

void L3Localizer::printBubbleList(void){

    for (int k=0; k<this->BubbleList.size(); k++){

        this->BubbleList[k]->printAllXY();

    }

}

void L3Localizer::LocalizeOMatic(std::string imageStorePath)
{
    //debugShow(this->TrainedData->TrainedAvgImage);
    cv::Mat sigmaImageRaw = this->TrainedData->TrainedSigmaImage;
    //sigmaImageRaw *= 10;
    //debugShow(sigmaImageRaw);
    /*Check for malformed events*/

    if (this->CameraFrames.size()<=20) this->okToProceed=false;

    for (int i=this->MatTrigFrame; i<=this->MatTrigFrame+6; i++){
        if (getFilesize(this->ImageDir + this->CameraFrames[i]) < 900000) {
            this->okToProceed=false;
            this->TriggerFrameIdentificationStatus = -10;
            std::cout<<"Failed analyzing event at: "<<this->ImageDir<<this->CameraFrames[i]<<"\n";
        }
    }


    /* ******************************** */



    if (!this->okToProceed) return;
    /*Assign the three useful frames*/
    if (this->CameraNumber==2) this->MatTrigFrame+=1;


    this->triggerFrame = cv::imread(this->ImageDir + this->CameraFrames[this->MatTrigFrame+1],0);
    this->presentationFrame = triggerFrame.clone();
    //cv::cvtColor(this->presentationFrame, this->presentationFrame, cv::COLOR_GRAY2BGR);
    this->ComparisonFrame = cv::imread(this->ImageDir + this->CameraFrames[0],0);

    /*Run the analyzer series*/
    if (this->CameraNumber==2) {
        this->CalculateInitialBubbleParamsCam2();


        if (this->MatTrigFrame<29){
            for (int k=1; k<=NumFramesBubbleTrack; k++)
            this->CalculatePostTriggerFrameParamsCam2(k);
        } else {
            for (int k=1; k<=(39-this->MatTrigFrame); k++)
            this->CalculatePostTriggerFrameParamsCam2(k);

        }


        //this->CalculatePostTriggerFrameParamsCam2(2);
        //this->CalculatePostTriggerFrameParamsCam2(3);
    } else {
        this->CalculateInitialBubbleParams();

        if (this->MatTrigFrame<29){
            for (int k=1; k<=NumFramesBubbleTrack; k++)
            this->CalculatePostTriggerFrameParams(k);
        } else {
            for (int k=1; k<=(39-this->MatTrigFrame); k++)
            this->CalculatePostTriggerFrameParams(k);

        }
        //this->CalculatePostTriggerFrameParams(2);
        //this->CalculatePostTriggerFrameParams(3);
    }



    //this->printBubbleList();
    //this->numBubbleMultiplicity=0;

    /*Analyze results*/
    //std::cout<<"Refined bubble multiplicity:  "<<this->numBubbleMultiplicity<<"\n";

    /*Store the finished image*/
    //cv::imwrite(imageStorePath+"/"+eventSeq+".jpg", BubbleFrame);

}
