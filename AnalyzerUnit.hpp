#ifndef ANALYZERUNIT_HPP_INCLUDED
#define ANALYZERUNIT_HPP_INCLUDED

#include <vector>
#include <string>
#include "AlgorithmTraining/Trainer.hpp"

#include <opencv2/opencv.hpp>
#include "bubble/bubble.hpp"


class AnalyzerUnit{

    private:

        /*Error handling stuff*/
        int StatusCode=0;



        /*Training frames and detection frames*/
        int minEvalFrameNumber = 2;
        int firstTrainingFrames = 1;

        float calculateEntropyFrame(cv::Mat& );

    protected:
        /*Event identification and location*/
        std::string ImageDir;
        std::string EventID;
        std::string MaskDir;

        /*List of all the frames belonging to the particular event in question*/
        std::vector<std::string> CameraFrames;
        int CameraNumber;




    public:
        /*Constructor and deconstructor*/
        AnalyzerUnit(std::string, std::string, int, Trainer**, std::string );
        ~AnalyzerUnit(void );

        /*Function to parse and sort the triggers from the folder and the directory where the images are stored*/
        void ParseAndSortFramesInFolder( void );

        /*Produces the text output for PICO format*/
        void ProduceOutput(void );

        /*Variable holding the RotatedRect bubble array and the trigger frame*/
        std::vector<cv::RotatedRect> BubblePixelPos;
        int MatTrigFrame;

        /*Find the trigger frame function*/
        void FindTriggerFrame(void);

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
bool frameSortFunc(std::string , std::string );


#endif // ANALYZERUNIT_HPP_INCLUDED
