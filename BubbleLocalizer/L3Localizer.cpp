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


L3Localizer::L3Localizer(std::string EventID, std::string ImageDir, int CameraNumber, bool nonStopPref, Trainer** TrainedData, std::string MaskDir, Parser* Parser):AnalyzerUnit(EventID, ImageDir, CameraNumber, TrainedData, MaskDir, Parser)
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

    if (!this->nonStopMode) std::cout << "-----Start ev " << this->EventID << ", cam " << CameraNumber << ", frame " << this->MatTrigFrame+30 << "-----" << std::endl;

    int blur_diam = 5;

    /*Construct the frame differences and LBPImage Frames*/
    cv::Mat overTheSigma, thresFrame;
    //cv::Mat NewFrameDiffTrig, TempFrameDiffTrig, TempFrameDiffPreTrig; //Not used; replaced by ProcessFrame
    //cv::absdiff(this->triggerFrame, this->TrainedData->TrainedAvgImage, TempFrameDiffTrig);
    //cv::absdiff(this->preTrigFrame, this->TrainedData->TrainedAvgImage, TempFrameDiffPreTrig);
    //cv::absdiff(TempFrameDiffTrig, TempFrameDiffPreTrig, NewFrameDiffTrig);
    ProcessFrame(this->triggerFrame, this->preTrigFrame, overTheSigma,blur_diam);

    /*Debug*/
    // DebugPeek must be created by user for these to be saved
    if (!this->nonStopMode){
        cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_000_AvgImage.png", this->TrainedData->TrainedAvgImage);
        cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_00_PreTrigFrame.png", this->preTrigFrame);
        cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_0_TrigFrame.png", this->triggerFrame);
        //cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_0001_TrigTrainAbsDiff.png", TempFrameDiffTrig);
        //cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_001_PreTrigTrainAbsDiff.png", TempFrameDiffPreTrig);
        //cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_01_TotalAbsDiff.png", NewFrameDiffTrig);
//        std::cout << "DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_1_TrigTrainAbsDiff.png" << std::endl;
    }

    //overTheSigma = NewFrameDiffTrig - 6./sqrt(2)*this->TrainedData->TrainedSigmaImage;    //Now done in ProcessFrame

    /*Debug*/
    if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_02_OvrThe6Sigma.png", overTheSigma);

    //cv::blur(overTheSigma,overTheSigma, cv::Size(3,3));    //Now done in ProcessFrame
    cv::threshold(overTheSigma, thresFrame, this->loc_thres, 255, cv::THRESH_TOZERO);
    if (!this->nonStopMode) std::cout << "this->loc_thres: " << this->loc_thres << std::endl;
    cv::threshold(thresFrame, thresFrame, 0, 255, cv::THRESH_BINARY|cv::THRESH_OTSU);

    /*Debug*/
    if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_3_OtsuThresholded.png", thresFrame);




    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(thresFrame, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1); 
    if (!this->nonStopMode) std::cout << contours.size() << std::endl;
    
    

    /*Make two vectors to store the fitted rectanglse and ellipses*/
    //std::vector<cv::RotatedRect> minAreaRect( contours.size() );
    std::vector<cv::Rect> minRect( contours.size() );

    int BoxArea=0;
    int largestBoxArea = 0;
    bool allInBellowsMask = contours.size()>0 ? true : false;
    /*Generate the ellipses and rectangles for each contours*/
    for( int i = 0; i < contours.size(); i++ ) {
        minRect[i] = cv::boundingRect( contours[i]);
        //minAreaRect[i] = cv::minAreaRect(contours[i] );
        BoxArea = minRect[i].width*minRect[i].height;
        if (!this->isInMask(&minRect[i],true)){  //If not in bellows region
            allInBellowsMask = false;
            if (largestBoxArea < BoxArea) largestBoxArea = BoxArea;
        }
        else{
            if (!nonStopMode) std::cout << "Found bubble in bellows mask." << std::endl;
            contours.erase(contours.begin()+i);
            i--;
        }
    }
    
    if (allInBellowsMask){  //Subtract bellows movement and see if contours remain
    
        std::string path = this->MaskDir + "/cam" + std::to_string(this->CameraNumber) + "_bellows_template.png";
        cv::Mat TemplateImage = cv::imread(path , cv::IMREAD_GRAYSCALE);
        if (!nonStopMode) cv::imwrite("DebugPeek/cam" + std::to_string(CameraNumber)+"_bellows_template.png", TemplateImage);
        if (TemplateImage.empty()){
            std::cout << "Template image not loadable for event " << this->EventID
                << " camera " << this->CameraNumber << "; cannot veto bellows movement triggers" << std::endl;
            std::cout << "Template path: " << path << std::endl;
        }
        
        else{
            if (!nonStopMode) std::cout << "Making empty frames" << std::endl;
            cv::Mat trig_copy = cv::Mat::zeros(this->triggerFrame.rows, this->triggerFrame.cols, this->triggerFrame.type());
            cv::Mat trig_copy_extra = cv::Mat::zeros(this->triggerFrame.rows, this->triggerFrame.cols, this->triggerFrame.type());
            cv::Mat preTrig_copy = cv::Mat::zeros(this->preTrigFrame.rows, this->preTrigFrame.cols, this->preTrigFrame.type());
            
            if (!nonStopMode) std::cout << "Matching bellows templates" << std::endl;
            cv::Point2f template_pos_trig;
            cv::Point2f template_pos_trig_extra;
            cv::Point2f template_pos_pretrig;
            TrackAFeature(this->triggerFrame,TemplateImage,template_pos_trig);
            TrackAFeature(this->preTrigFrame,TemplateImage,template_pos_pretrig);
            
            if (!nonStopMode) std::cout << "Copying templates to empty frames" << std::endl;
            template_pos_trig_extra = template_pos_trig;
            if (int(template_pos_trig.x) == int(template_pos_pretrig.x) && int(template_pos_trig.y) == int(template_pos_pretrig.y)){
                if (!nonStopMode) std::cout << "No motion seen" << std::endl;
                template_pos_trig.x -= 1;
                template_pos_trig_extra.x += 1;
            }
            else{
                template_pos_trig_extra.x += ceil(template_pos_trig.x - template_pos_pretrig.x);
                template_pos_trig_extra.y += ceil(template_pos_trig.y - template_pos_pretrig.y);
                if (!nonStopMode) std::cout << "Motion seen: delta X: " << template_pos_trig.x-template_pos_pretrig.x << ", delta Y: " << template_pos_trig.y-template_pos_pretrig.y << std::endl;
            }
            if (!this->nonStopMode){
                std::cout << "template_pos_trig.x: " << template_pos_trig.x << "; template_pos_trig.y: " << template_pos_trig.y << std::endl;
                std::cout << "template_pos_pretrig.x: " << template_pos_pretrig.x << "; template_pos_pretrig.y: " << template_pos_pretrig.y << std::endl;
                std::cout << "template_pos_trig_extra.x: " << template_pos_trig_extra.x << "; template_pos_trig_extra.y: " << template_pos_trig_extra.y << std::endl;
            }
            
            TemplateImage.copyTo(trig_copy(cv::Rect(template_pos_trig.x,template_pos_trig.y,TemplateImage.cols, TemplateImage.rows)));
            TemplateImage.copyTo(trig_copy_extra(cv::Rect(template_pos_trig_extra.x,template_pos_trig_extra.y,TemplateImage.cols, TemplateImage.rows)));
            TemplateImage.copyTo(preTrig_copy(cv::Rect(template_pos_pretrig.x,template_pos_pretrig.y,TemplateImage.cols, TemplateImage.rows)));
            if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_1_BellowsTrig.png", trig_copy);
            if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_1_BellowsTrigExtra.png", trig_copy_extra);
            if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_1_BellowsPreTrig.png", preTrig_copy);
            
            if (!nonStopMode) std::cout << "Processing new frames" << std::endl;
            cv::Mat diff_frame, diff_frame_extra;
            cv::Rect diffROI = GetDiffROI(template_pos_trig,template_pos_pretrig,TemplateImage);
            ProcessFrame(trig_copy,preTrig_copy,diff_frame,blur_diam,diffROI);
            diffROI = GetDiffROI(template_pos_trig_extra,template_pos_pretrig,TemplateImage);
            ProcessFrame(trig_copy_extra,preTrig_copy,diff_frame_extra,blur_diam,diffROI);
            diff_frame += diff_frame_extra;
            if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_002_BellowsDiff.png", diff_frame);
            
            if (!nonStopMode) std::cout << "Taking diff" << std::endl;
            overTheSigma -= diff_frame;
            if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_2_OvrThe6SigmaBellows.png", overTheSigma);
            
            cv::threshold(overTheSigma, thresFrame, this->loc_thres, 255, cv::THRESH_TOZERO);
            if (!this->nonStopMode) std::cout << "this->loc_thres: " << this->loc_thres << std::endl;
            cv::threshold(thresFrame, thresFrame, 0, 255, cv::THRESH_BINARY|cv::THRESH_OTSU);
            if (!this->nonStopMode) cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_3_OtsuThresholded.png", thresFrame);
        }
        
        if (!nonStopMode) std::cout << "Getting new contours" << std::endl;
        /*Use contour / canny edge detection to find contours of interesting objects*/
        contours.clear();
        cv::findContours(thresFrame, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1); 
        if (!this->nonStopMode) std::cout << contours.size() << std::endl;
        
        minRect.clear();
        minRect.resize( contours.size() );

        BoxArea=0;
        largestBoxArea = 0;
        /*Generate the ellipses and rectangles for each contours*/
        for( int i = 0; i < contours.size(); i++ ) {
            minRect[i] = cv::boundingRect( contours[i]);
            //minAreaRect[i] = cv::minAreaRect(contours[i] );
            BoxArea = minRect[i].width*minRect[i].height;
            if (!this->nonStopMode) std::cout << "Contour " << i << " box area: " << BoxArea << std::endl;
            if (largestBoxArea < BoxArea) largestBoxArea = BoxArea;
        }
    }
    
    for( int i = 0; i < contours.size(); i++ ) {
        BoxArea = minRect[i].width*minRect[i].height;
    
        if (BoxArea>10 || BoxArea >= largestBoxArea){
            if (!this->nonStopMode) std::cout<<" Bubble genesis              X: "<<minRect[i].x<<" Y: "<<minRect[i].y<<" W: "<<minRect[i].width<<" H: "<<minRect[i].height<<"\n";
            cv::rectangle(this->presentationFrame, minRect[i], cv::Scalar( 255,255,255)/*this->color_red*/,1,8,0);
            this->bubbleRects.push_back(minRect[i]);

            BubbleImageFrame _thisBubbleFrame;
            _thisBubbleFrame.ContArea = cv::contourArea(contours[i]);
            _thisBubbleFrame.newPosition = minRect[i];
            _thisBubbleFrame.moments = cv::moments(contours[i], false); /*second parameter is for a binary image*/
            _thisBubbleFrame.ContRadius = sqrt(_thisBubbleFrame.ContArea/3.14159);
            if (_thisBubbleFrame.moments.m00>0){
                _thisBubbleFrame.MassCentres = cv::Point2f( _thisBubbleFrame.moments.m10/_thisBubbleFrame.moments.m00 ,
                                                            _thisBubbleFrame.moments.m01/_thisBubbleFrame.moments.m00);
            }
            else{
                double x = 0, y = 0, n = 0;
                for (int cc = 0; cc < contours[i].size(); cc++){
                  x += contours[i][cc].x;
                  y += contours[i][cc].y;
                  n++;
                }
                x /= n;
                y /= n;
                _thisBubbleFrame.MassCentres = cv::Point2f( x,y );
            }
            if (!this->nonStopMode){
                std::cout << "contours[" << i << "].size(): " << contours[i].size() << std::endl;
                std::cout << "X: " << _thisBubbleFrame.moments.m10/_thisBubbleFrame.moments.m00 << " Y: " << _thisBubbleFrame.moments.m01/_thisBubbleFrame.moments.m00 << std::endl;
                std::cout << "_thisBubbleFrame.moments.m00: " << _thisBubbleFrame.moments.m00 << std::endl;
                std::cout << "_thisBubbleFrame.moments.m10: " << _thisBubbleFrame.moments.m10 << std::endl;
                std::cout << "_thisBubbleFrame.moments.m01: " << _thisBubbleFrame.moments.m01 << std::endl;
                std::cout << "_thisBubbleFrame.ContArea: " << _thisBubbleFrame.ContArea << std::endl;
                std::cout << "_thisBubbleFrame.ContRadius: " << _thisBubbleFrame.ContRadius << std::endl;
                std::cout << "_thisBubbleFrame.MassCentres: " << _thisBubbleFrame.MassCentres.x << "," << _thisBubbleFrame.MassCentres.y << std::endl;
            }

            //Checking if the genesis coordinates of the bubble are within acceptable area (cam_mask)
            if (!(this->isInMask(&_thisBubbleFrame.newPosition))) {
                //if not, we continue and check next bubble without adding it to Bubblelist
                if (!this->nonStopMode) std::cout << "Skipping genesis bubble..." << std::endl;
                continue;
            }

            //bubble* firstBubble = new bubble(minAreaRect[i]);
            bubble* firstBubble = new bubble(_thisBubbleFrame);
            this->BubbleList.push_back(firstBubble);

        }

    }

    /*Debug*/
    if (!this->nonStopMode){
      cv::imwrite("DebugPeek/ev" + this->EventID + "_cam" + std::to_string(CameraNumber)+"_4_BubbleDetected.png", this->presentationFrame);
      std::cout << "-----End ev " << this->EventID << ", cam " << CameraNumber << ", frame " << this->MatTrigFrame+30 << "-----" << std::endl;
    }

    //NewFrameDiffTrig.refcount=0;
    //overTheSigma.refcount=0;
    //NewFrameDiffTrig.release();
    overTheSigma.release();
    thresFrame.release();
    //TempFrameDiffTrig.release();
    //TempFrameDiffPreTrig.release();
    //debugShow(this->presentationFrame);
}

cv::Rect L3Localizer::GetDiffROI(cv::Point2f point1, cv::Point2f point2, cv::Mat& frame){
    int start_x, start_y, delta_x, delta_y;
    start_x = std::max(point1.x,point2.x);
    start_y = std::max(point1.y,point2.y);
    delta_x = std::min(point1.x,point2.x) + frame.cols - start_x;
    delta_y = std::min(point1.y,point2.y) + frame.rows - start_y;
    if (!this->nonStopMode) std::cout << "ROI: X1=" << start_x << "; X2=" << start_x+delta_x << "; Y1=" << start_y << "; Y2=" << start_y+delta_y << std::endl;
    return cv::Rect(start_x,start_y,delta_x,delta_y);
}


void L3Localizer::TrackAFeature(cv::Mat& frame, cv::Mat TemplateImage, cv::Point2f& BestMatchLoc){

    cv::Mat result;

    /*Restrict the ROT of the analysisFrame*/
    //TODO: Remove hard-coding after testing the speed of this process and expanding ROI (potentially up to full image or some distance from 255 bin in bellows mask)
    cv::Rect TemplateSearchZone;
    if (CameraNumber == 3)
        TemplateSearchZone = cv::Rect(1324-50, 344-10, TemplateImage.cols+100, TemplateImage.rows+20);
    if (CameraNumber == 1)
        TemplateSearchZone = cv::Rect(1367-50, 253-10, TemplateImage.cols+100, TemplateImage.rows+20);
    if (!nonStopMode) std::cout << "Making ROI" << std::endl;
    cv::Mat _AnaFrameSmallROI = cv::Mat(frame, TemplateSearchZone);



    // Create the result matrix
    if (!nonStopMode) std::cout << "Create the result matrix" << std::endl;
    int result_cols =  _AnaFrameSmallROI.cols - TemplateImage.cols + 1;
    int result_rows = _AnaFrameSmallROI.rows - TemplateImage.rows + 1;
    result.create( result_rows, result_cols, CV_32F );


    // Do the Matching and Normalize
    if (!nonStopMode) std::cout << "Do the Matching and Normalize" << std::endl;
    int match_method=CV_TM_CCORR_NORMED;
    cv::matchTemplate( _AnaFrameSmallROI, TemplateImage, result, match_method );
    cv::normalize( result, result, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );



    // Localizing the best match with minMaxLoc
    if (!nonStopMode) std::cout << "Localizing the best match with minMaxLoc" << std::endl;
    double minVal; double maxVal; cv::Point minLoc; cv::Point maxLoc;
    cv::Point matchLoc;
    cv::minMaxLoc( result, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat() );

    // For SQDIFF and SQDIFF_NORMED, the best matches are lower values. For all the other methods, the higher the better
    if( match_method  == CV_TM_SQDIFF || match_method == CV_TM_SQDIFF_NORMED )
        { matchLoc = minLoc; }
    else
        { matchLoc = maxLoc; }

    /*Append result to return var*/

    cv::Point2f BestMatchSubPixel;
    BestMatchSubPixel = cv::Point2f(0.0,0.0);

    float total_mass=0.0;
    float inv_mass=0.0;
    float pixel_value=0.0;
    
    int SubPixelAveragingGrid = 1;

    for (int i=-SubPixelAveragingGrid; i<=SubPixelAveragingGrid; i++){
        for (int j=-SubPixelAveragingGrid; j<=SubPixelAveragingGrid; j++){
            pixel_value = result.at<float>(matchLoc.y+j, matchLoc.x+i);
            BestMatchSubPixel+=cv::Point2f(matchLoc.x+i,matchLoc.y+j)*pixel_value;
            total_mass +=pixel_value;
        }
    }

    inv_mass = 1.0/total_mass;
    BestMatchSubPixel *= inv_mass;



    /*Offset correction*/
    BestMatchLoc = cv::Point2f(BestMatchSubPixel.x+TemplateSearchZone.x, BestMatchSubPixel.y+TemplateSearchZone.y);
    //std::cout<<BestMatchLoc<<"\n";
    result.release();
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
    cv::threshold(overTheSigma, bubMinusShadow, 100, 255, cv::THRESH_TOZERO|cv::THRESH_OTSU);


    //debugShow(bubMinusShadow);

    /*Get rid of pixel noise*/
    cv::threshold(bubMinusShadow, bubMinusShadow, 10, 255, cv::THRESH_TOZERO);

    //debugShow(bubMinusShadow);
    /*Check if this is a trigger by the interface moving or not - Note: Works ONLY on cam 2's entropy settings*/
    //showHistogramImage(bubMinusShadow);
    float ImageDynamicRange = ImageDynamicRangeSum(bubMinusShadow,60,200);
    if (ImageDynamicRange==0.0) return;

    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(bubMinusShadow, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1);

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

            //Checking if the genesis coordinates of the bubble are within acceptable area (cam_mask)
            if (!(this->isInMask(&_thisBubbleFrame.newPosition))) {
                //if not, we continue and check next bubble without adding it to Bubblelist
                continue;
            }


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
    //this->PostTrigWorkingFrame = cv::imread(this->ImageDir + this->CameraFrames[this->MatTrigFrame+1+postTrigFrameNumber],0);
    this->FileParser->GetImage(this->EventID, this->CameraFrames[this->MatTrigFrame+1+postTrigFrameNumber], this->PostTrigWorkingFrame);
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
    cv::threshold(overTheSigma, bubMinusShadow, 100, 255, cv::THRESH_TOZERO|cv::THRESH_OTSU);




    /*Get rid of pixel noise*/
    cv::threshold(bubMinusShadow, bubMinusShadow, 10, 255, cv::THRESH_TOZERO);

    /*Check if this is a trigger by the interface moving or not - Note: Works ONLY on cam 2's entropy settings*/
    //showHistogramImage(bubMinusShadow);
    float ImageDynamicRange = ImageDynamicRangeSum(bubMinusShadow,100,200);
    if (ImageDynamicRange==0.0) return;


    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(bubMinusShadow, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1);

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

            //Checking if the feature coordinates are within acceptable area (cam_mask)
            if (!(this->isInMask(&_thisBubbleFrame.newPosition))) {
                //if not, we continue and check next bubble without adding it to Bubblelist
                continue;
            }

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
    //this->PostTrigWorkingFrame = cv::imread(this->ImageDir + this->CameraFrames[this->MatTrigFrame+postTrigFrameNumber],0);
    this->FileParser->GetImage(this->EventID, this->CameraFrames[this->MatTrigFrame+postTrigFrameNumber], this->PostTrigWorkingFrame);
    //tempPresentation =  this->PostTrigWorkingFrame.clone();
    //cv::cvtColor(tempPresentation, tempPresentation, cv::COLOR_GRAY2BGR);




    /*Construct the frame differences*/
    cv::Mat NewFrameDiffTrig, overTheSigma, LBPImageTrigBeforeBlur, LBPImageTrigAfterBlur;
    cv::absdiff(this->PostTrigWorkingFrame, this->TrainedData->TrainedAvgImage, NewFrameDiffTrig);

    /*Calculate pixels over the sigma*/
    overTheSigma = NewFrameDiffTrig - 6*this->TrainedData->TrainedSigmaImage;

    /*Blur and threshold to remove pixel noise*/
    cv::blur(overTheSigma,overTheSigma, cv::Size(3,3));
    cv::threshold(overTheSigma, overTheSigma, 3, 255, cv::THRESH_TOZERO);
    cv::threshold(overTheSigma, overTheSigma, 0, 255, cv::THRESH_BINARY|cv::THRESH_OTSU);



    /*Use contour / canny edge detection to find contours of interesting objects*/
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(overTheSigma, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_TC89_L1);

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

            //Checking if the coordinates of the feature are within acceptable area (cam_mask)
            if (!(this->isInMask(&_thisBubbleFrame.newPosition))) {
                //if not, we continue and check next bubble without adding it to Bubblelist
                continue;
            }

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

    if (this->CameraFrames.size()<=5) this->okToProceed=false;

    /* Doesn't seem necessary...
     * The following loop basically only checks if the trigger frame is
     * within 6 frames of the final frame in the set, but that can be checked
     * while analyzing the data.
    */
    /*
    for (int i=this->MatTrigFrame; i<=this->MatTrigFrame+6; i++){
        if (i >= this->CameraFrames.size()){
            this->okToProceed=false;
            break;
        }
        if (getFilesize(this->ImageDir + this->CameraFrames[i]) < MIN_IMAGE_SIZE) {
            this->okToProceed=false;
            this->TriggerFrameIdentificationStatus = -10;
            std::cout<<"Failed analyzing event at: "<<this->ImageDir<<this->CameraFrames[i]<<"\n";
        }
    }
    */


    /* ******************************** */



    if (!this->okToProceed) return;
    /*Assign the three useful frames*/
//    if (this->CameraNumber==2) this->MatTrigFrame+=1;

    int prev_offset = 2;
    //Slight pressure changes over the course of the run change the local density of the freon, which results in some intensity fluctuations near the bellows.
    //This can trigger the algorithm at the beginning of the event, so we sacrifice some sensitivity at the beginning of the event to compensate for a lack of sigma frame.
    //It's possible to have a trigger in the first frames as well, and there's no easy way to tell the difference other than to reduce the frame difference.
    if (this->TrainedData->TrainingSetSize < 6){
        prev_offset = 1;
        if (!this->nonStopMode) std::cout << "Setting prev_offset to " << prev_offset << std::endl;
    }

    //this->triggerFrame = cv::imread(this->ImageDir + this->CameraFrames[this->MatTrigFrame],0);
    this->FileParser->GetImage(this->EventID, this->CameraFrames[this->MatTrigFrame], this->triggerFrame);
    this->FileParser->GetImage(this->EventID, this->CameraFrames[this->MatTrigFrame-prev_offset], this->preTrigFrame);    //Use the frame two frames back in case bubble actually started emerging in trig-1
    this->presentationFrame = triggerFrame.clone();
    //cv::cvtColor(this->presentationFrame, this->presentationFrame, cv::COLOR_GRAY2BGR);
    //this->ComparisonFrame = cv::imread(this->ImageDir + this->CameraFrames[0],0);
    this->FileParser->GetImage(this->EventID, this->CameraFrames[0], this->ComparisonFrame);

    /*Run the analyzer series*/
    this->CalculateInitialBubbleParams();

    if (this->MatTrigFrame<29){
        for (int k=1; k<=NumFramesBubbleTrack; k++){
            if ( (this->MatTrigFrame + k) >= this->CameraFrames.size() ) break;
            this->CalculatePostTriggerFrameParams(k);
        }
    } else {
        for (int k=1; k<=(39-this->MatTrigFrame); k++){
            if ( (this->MatTrigFrame + k) >= this->CameraFrames.size() ) break;
            this->CalculatePostTriggerFrameParams(k);
        }
    }




    //this->printBubbleList();
    //this->numBubbleMultiplicity=0;

    /*Analyze results*/
    //std::cout<<"Refined bubble multiplicity:  "<<this->numBubbleMultiplicity<<"\n";

    /*Store the finished image*/
    //cv::imwrite(imageStorePath+"/"+eventSeq+".jpg", BubbleFrame);

}

//filtering out genesis locations that occur out of acceptable area according to masks in external folder.
bool L3Localizer::isInMask( cv::Rect *genesis_coords, bool bellows )
{

    int xpix = genesis_coords->x+genesis_coords->width/2.;
    int ypix = genesis_coords->y+genesis_coords->height/2;
    //This is why cam_masks has to be in the build dir -- this path is relative to the executable.
    cv::Mat mask_image;
    std::string path = this->MaskDir + "/cam" + std::to_string(this->CameraNumber);
    if (bellows) path += "_bellows";
    path += "_mask.bmp";
    if (bellows && bellows_mask.empty()) this->bellows_mask = cv::imread(path , cv::IMREAD_GRAYSCALE);
    else if (!bellows && cam_mask.empty()) this->cam_mask = cv::imread(path , cv::IMREAD_GRAYSCALE);
    if (bellows) mask_image = bellows_mask;
    else mask_image = cam_mask;
    if (mask_image.empty()){
        std::cout << "Mask image not loadable for event " << this->EventID
            << " camera " << this->CameraNumber << "; skipping mask check" << std::endl;
        if (!bellows) return true;
        else return false;
    }
//    cv::Mat mask_image = cv::imread("./cam_masks/cam" + std::to_string(this->CameraNumber) + "_mask.bmp" , cv::IMREAD_GRAYSCALE);

    /*the cam masks we currently have are grayscale bitmaps meaning we either have a 255 or 0 pixel value. (it's one bit but for some reason it is 255)
    If we land in the black (out of bounds) zone, ie. 0, we are out of bounds.*/

    //c::mat.at() takes in (y,x) NOT (x,y). Why? It is a matrix, hence we have: row,col (y,x).
    if ((int)mask_image.at<uchar>(ypix,xpix) > 0 ) {

        return true; //It is within the acceptable range

    } else {

        if (!this->nonStopMode && !bellows) std::cout << "\nCaught a bubble at: (" << xpix << "," <<  ypix << ") cam " << this->CameraNumber <<", event " << this-> EventID << ") with pixel value: " << (int)mask_image.at<uchar>(ypix,xpix) << " (out of image mask bounds)\n";
        return false; //It lies outside of the acceptable range
    }


}



