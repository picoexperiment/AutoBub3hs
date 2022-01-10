#ifndef PICOWRITER_HPP_INCLUDED
#define PICOWRITER_HPP_INCLUDED

#include <vector>
#include <string>
#include <fstream>
#include <iostream>

#include <opencv2/opencv.hpp>
#include "../bubble/bubble.hpp"



class OutputWriter{

    private:

        int camera;
        int frameOffset;
        int StatusCode;
        std::string OutputDir;
        std::string run_number;
        std::string abubOutFilename;

        std::ofstream OutFile;
        std::stringstream _StreamOutput;


    protected:


    public:

        struct BubbleData{
            std::vector<bubble*> BubbleObjectData;
            int StatusCode;
            int frame0;
            int event;
            float dzdt;
            float drdt;
            bool written;
            BubbleData();
        };
        /*direct access to the trained data*/
        BubbleData BubbleData0;
        BubbleData BubbleData1;
        BubbleData BubbleData2;
        BubbleData BubbleData3;


        /*Trainer instance initilized with the camera number*/
        OutputWriter(std::string, std::string, int );
        ~OutputWriter(void );

        /*Compute the mean and std*/
        void writeHeader(void );
        void stageCameraOutput(std::vector<bubble*> , int, int, int);
        void stageCameraOutputError(int, int, int);

        void formEachBubbleOutput(int, int&, int );
        void writeCameraOutput(void);
        int CalculateNBubCamera(int );



};


#endif // ANALYZERUNIT_HPP_INCLUDED
