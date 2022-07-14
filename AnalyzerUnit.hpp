#ifndef ANALYZERUNIT_HPP_INCLUDED
#define ANALYZERUNIT_HPP_INCLUDED

#include <vector>
#include <string>
#include "AlgorithmTraining/Trainer.hpp"

#include <opencv2/opencv.hpp>
#include "bubble/bubble.hpp"
#include "ParseFolder/Parser.hpp"
#include "ParseFolder/RawParser.hpp"
#include "ParseFolder/ZipParser.hpp"

#define MIN_IMAGE_SIZE 100000

class AnalyzerUnit{

    private:

        /*Error handling stuff*/
        int StatusCode=0;



        /*Training frames and detection frames*/
        int minEvalFrameNumber = 0;     //CR: Not sure why we need this, so changing from 2 to 0
        int firstTrainingFrames = 1;    //CR: This doesn't seem to do anything anymore
        int loc_thres_max = 3;          //PICO-60 default for loc_thres. Increase if getting a lot of false positives

        float calculateEntropyFrame(cv::Mat& , bool debug = false);
        double calculateSignificanceFrame(cv::Mat& ImageFrame, bool store, bool debug = false);

    protected:
        /*Event identification and location*/
        std::string ImageDir;
        std::string EventID;
        std::string MaskDir;

        /*List of all the frames belonging to the particular event in question*/
        std::vector<std::string> CameraFrames;
        int CameraNumber;

        Parser *FileParser;

    public:
        /*Constructor and deconstructor*/
        AnalyzerUnit(std::string, std::string, int, Trainer**, std::string, Parser* );
        ~AnalyzerUnit(void );

        /*Function to parse and sort the triggers from the folder and the directory where the images are stored*/
        void ParseAndSortFramesInFolder( void );

        /*Produces the text output for PICO format*/
        void ProduceOutput(void );
        
        /*Perform subtraction and blurring on image before checking for trigger*/
        void ProcessFrame(cv::Mat& workingFrame, cv::Mat& prevFrame, cv::Mat& subtr_frame, int blur_diam = 5, int img_num = -1);

        /*Variable holding the RotatedRect bubble array and the trigger frame*/
        std::vector<cv::RotatedRect> BubblePixelPos;
        int MatTrigFrame;
        
        /*Threshold for localizer*/
        int loc_thres;
        
        std::vector< std::vector<int> > pix_counts;

        /*Find the trigger frame function*/
        void FindTriggerFrame(bool nonStopMode, int startframe);

        /*Overloaded function based on the analyzer*/
        virtual void LocalizeOMatic(std::string )=0; //EventList[evi] is being passed. Why?
        std::vector<cv::Rect> bubbleRects;

        Trainer* TrainedData;
        /*Training Data*/
        std::vector<bubble*> BubbleList;

        /*Status checks*/
        bool okToProceed = true;
        int TriggerFrameIdentificationStatus=0; //0=OK | 1=Trigger on frame 1 | 2=No trigger found


};

/*Helper functions*/
template <typename num>
double CalcMean(std::vector<num> &vec, int size = -1);
template <typename num>
double CalcStdDev(std::vector<num> &vec, double mean, int size = -1);
bool frameSortFunc(std::string , std::string );
void sqrt_mat(cv::Mat& M);


#endif // ANALYZERUNIT_HPP_INCLUDED
