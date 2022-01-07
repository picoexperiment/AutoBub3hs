/* OpenCV code to find Ellipses for
 * PICO Dark Matter search - the Geyser
 *
 * Written by Pitam Mitra for PICO Collaboration
 *
 * Latest Edit: 2014-07-11. See the HG Changelog
 */


#include <opencv2/opencv.hpp>
//#include "opencv2/cudaimgproc.hpp"
//#include <opencv/highgui/highgui.hpp>
//#include <opencv2/core/core.hpp>

/*C++ Stuff*/
#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>     /* exit, EXIT_FAILURE */
#include <stdexcept>

/*Geyser Image Analysis Stuff*/
#include "ParseFolder/ParseFolder.hpp"
//#include "common/CommonDatatypes.h"
//#include "SQLiteDBstorage/imageAnalysisResults.hpp"
#include "BubbleLocalizer/L3Localizer.hpp"
#include "LBP/lbp.hpp"
#include "LBP/LBPUser.hpp"

#include "AnalyzerUnit.hpp"
#include "AlgorithmTraining/Trainer.hpp"
#include "PICOFormatWriter/PICOFormatWriterV3.hpp"
#include "bubble/bubble.hpp"
#include "common/UtilityFunctions.hpp"

#include <omp.h>

const int evalEntropyThresholdFrames = 2;
std::vector<int> badEvents;





/*Workaround because fermi grid is using old gcc*/
bool BubblePosZsort(cv::RotatedRect a, cv::RotatedRect b)
{
    return a.center.y<b.center.y;
}
bool eventNameOrderSort(std::string a, std::string b)
{
    return std::stoi(a)<std::stoi(b);
}


/*This is the same routine run serially on all cams. It was stupid to do the same ops 4 times,
 *this simplifies the run*/

int AnyCamAnalysis(std::string EventID, std::string ImgDir, int camera, bool nonStopPref, Trainer** TrainingData,
                    OutputWriter** P60Output, std::string out_dir, int actualEventNumber, AnalyzerUnit** AGeneric){


    OutputWriter* Pico60Writer = *P60Output;

    //AnalyzerUnit *AnalyzerC0 = new L3Localizer(EventList[evi], imageDir, 0, true, &TrainC0); /*EventID, imageDir and camera number*/
    //AnalyzerUnit *AnalyzerCGeneric = new L3Localizer(EventID, ImgDir, camera, true, TrainingData); /*EventID, imageDir and camera number*/
    AnalyzerUnit *AnalyzerCGeneric = *AGeneric;


    /* ***************************
     * ***** Camera Operations ******
     ********************************/

    /*Exception handling - per camera*/
    try
    {
        AnalyzerCGeneric->ParseAndSortFramesInFolder();
        AnalyzerCGeneric->FindTriggerFrame();
        //cout<<"Trigger Frame: "<<AnalyzerCGeneric->MatTrigFrame<<"\n";
        //std::cout << actualEventNumber << " " << camera << " " << AnalyzerCGeneric->MatTrigFrame <<  " " << AnalyzerCGeneric->TriggerFrameIdentificationStatus << std::endl;
        if (AnalyzerCGeneric->okToProceed)
        {

            AnalyzerCGeneric->LocalizeOMatic(out_dir);  //uncomment for full run
            if (AnalyzerCGeneric->okToProceed) {
                #pragma omp critical
                Pico60Writer->stageCameraOutput(AnalyzerCGeneric->BubbleList, camera, AnalyzerCGeneric->MatTrigFrame, actualEventNumber);
            }
            else {
                Pico60Writer->stageCameraOutputError(camera,-8, actualEventNumber);
                }
        }
        else
        {
            Pico60Writer->stageCameraOutputError(camera,AnalyzerCGeneric->TriggerFrameIdentificationStatus, actualEventNumber);
        }

    /*The exception block for camera specific crashes. outputs -6 for the error*/
    }
    catch (exception& e)
    {
        std::cout << e.what() << '\n';
        Pico60Writer->stageCameraOutputError(camera,-6, actualEventNumber);

    }

    //delete AnalyzerCGeneric;
    advance_cursor(); /*Fancy coursors!*/
    return 0;

}


/*The main autobub code starts here*/
int main(int argc, char** argv)
{

    printf("This is AutoBub v3, the automatic unified bubble finder code for all chambers\n");

    if (argc < 3)
    {
        printf("Not enough parameters.\nUsage: abub <location of data> <run number> <directory for output file>\nEg: abub /coupp/data/30l-13/ 20140501_0 /home/coupp/recon/\n");
        printf("Note the trailing slashes.\n");
        return -1;
    }

    std::string dataLoc = argv[1];
    std::string run_number = argv[2];
    std::string out_dir = argv[3];

    std::string mask_dir = "";
    if (argc>=5){ mask_dir = argv[4]; }

    std::string data_series = "";
    if (argc>=6){ data_series = argv[5]; }

    std::string eventDir = dataLoc + run_number + "/";


    /*I anticipate the object to become large with many bubbles, so I wanted it on the heap*/
    OutputWriter *PICO60Output = new OutputWriter(out_dir, run_number);
    PICO60Output->writeHeader();

    /*Construct list of events*/
    std::vector<std::string> EventList;
    int* EVstatuscode = 0;

    try
    {
        GetEventDirLists(eventDir.c_str(), EventList, EVstatuscode);

    /*Crash handler at the begining of the program - writes -5 if the folder could not be read*/
    }
    catch (...)
    {
        std::cout<<"Failed to read the images from the run. Autobub cannot continue.\n";
        PICO60Output->stageCameraOutputError(0,-5, -1);
        PICO60Output->stageCameraOutputError(1,-5, -1);
        PICO60Output->stageCameraOutputError(2,-5, -1); //cam2,3 absent in data now
        PICO60Output->stageCameraOutputError(3,-5, -1);
        PICO60Output->writeCameraOutput();
        return -5;
    }

    /*A sort is unnecessary at this level, but it is good practice and does not cost extra resources*/
    std::sort(EventList.begin(), EventList.end(), eventNameOrderSort);
    /*Event list is now constructed*/


    /* The following variables deal with the different ways that images have been
     * saved in different experiments.
     */
    std::string imageFormat;
    std::string imageFolder;
    if (data_series=="01l-21" | data_series=="2l-16"){
            imageFormat = "cam%dimage %u.bmp";
            imageFolder = "/";
    }
    else {
            imageFormat = "cam%d_image%u.png";
            imageFolder = "/Images/";
    }

    std::cout << imageFormat << " " << imageFolder << std::endl;




    /*Learn Mode
     *Train on a given set of images for background subtract
     */
    printf("**Starting training. AutoBub is in learn mode**\n");
    Trainer *TrainC0 = new Trainer(0, EventList, eventDir, imageFormat, imageFolder);
    Trainer *TrainC1 = new Trainer(1, EventList, eventDir, imageFormat, imageFolder);
    Trainer *TrainC2 = new Trainer(2, EventList, eventDir, imageFormat, imageFolder);
    Trainer *TrainC3 = new Trainer(3, EventList, eventDir, imageFormat, imageFolder);

    //Trainer* trainers [4] = {TrainC0, TrainC1, TrainC2, TrainC3};


    try {
        #pragma omp parallel
        {
            #pragma omp single nowait
            {
                TrainC0->MakeAvgSigmaImage(false);
            }
            #pragma omp single nowait
            {
                TrainC1->MakeAvgSigmaImage(false);
            }
            #pragma omp single nowait
            {
                TrainC2->MakeAvgSigmaImage(false);
            }
            #pragma omp single nowait
            {
                TrainC3->MakeAvgSigmaImage(false);
            }
        }

//        TrainC0->MakeAvgSigmaImage(false);
//        TrainC1->MakeAvgSigmaImage(false);
//        TrainC2->MakeAvgSigmaImage(false); //cam2,3 absent in data now
//        TrainC3->MakeAvgSigmaImage(false);

    } catch (...) {
        std::cout<<"Failed to train on images from the run. Autobub cannot continue.\n";
        for (int evi=0; evi<EventList.size(); evi++){
            int actualEventNumber = atoi(EventList[evi].c_str());
            PICO60Output->stageCameraOutputError(0,-7, actualEventNumber);
            PICO60Output->stageCameraOutputError(1,-7, actualEventNumber);
            PICO60Output->stageCameraOutputError(2,-7, actualEventNumber); //cam 2,3 absent in data now
            PICO60Output->stageCameraOutputError(3,-7, actualEventNumber);
            PICO60Output->writeCameraOutput();
        }
        return -7;
    }


    printf("***Training complete. AutoBub is now in detect mode***\n");
    // Create a separate writer per event
    delete PICO60Output;


    /*Detect mode
     *Iterate through all the events in the list and detect bubbles in them one by one
     *A seprate procedure will store them to a file at the end
     */
    int num_threads_total = omp_get_max_threads();
    int num_threads_cam = 4;
    int num_threads_evs = int(num_threads_total / num_threads_cam);
    std::cout << "Total threads: " << num_threads_total << std::endl;
    #pragma omp parallel for ordered schedule(static, 1) num_threads(num_threads_total)
    for (int evi = 0; evi < EventList.size(); evi++)
    {
        OutputWriter *PICO60Output = new OutputWriter(out_dir, run_number);
        std::string imageDir=eventDir+EventList[evi]+"/Images/";
        /*We need the actual event number in case folders with events are missing*/
        int actualEventNumber = atoi(EventList[evi].c_str());

        printf("\rProcessing event: %s / %d  ... ", EventList[evi].c_str(), static_cast<int>(EventList.size())-1);
        advance_cursor(); /*Fancy coursors!*/

        /* ***************************
         * ***** Camera Operations ******
         ********************************/
        //#pragma omp ordered
        AnalyzerUnit *AnalyzerC0 = new L3Localizer(EventList[evi], imageDir, 0, true, &TrainC0, mask_dir);
        AnalyzerUnit *AnalyzerC1 = new L3Localizer(EventList[evi], imageDir, 1, true, &TrainC1, mask_dir);
        AnalyzerUnit *AnalyzerC2 = new L3Localizer(EventList[evi], imageDir, 2, true, &TrainC2, mask_dir);
        AnalyzerUnit *AnalyzerC3 = new L3Localizer(EventList[evi], imageDir, 3, true, &TrainC3, mask_dir);

        AnyCamAnalysis(EventList[evi], imageDir, 0, true, &TrainC0, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC0);
        AnyCamAnalysis(EventList[evi], imageDir, 1, true, &TrainC1, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC1);
        AnyCamAnalysis(EventList[evi], imageDir, 2, true, &TrainC2, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC2); //cam 2,3 absent in data now
        AnyCamAnalysis(EventList[evi], imageDir, 3, true, &TrainC3, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC3); //cam 2,3 absent in data now

        /*Write and commit output after each iteration, so in the event of a crash, its not lost*/
//        #pragma omp barrier
        #pragma omp ordered
        {
           PICO60Output->writeCameraOutput();
        }

        delete AnalyzerC0, AnalyzerC1, AnalyzerC2, AnalyzerC3; //cam 2,3 absent in data now
        delete PICO60Output;
    }

    printf("run complete.\n");

    /*GC*/
    delete TrainC0;
    delete TrainC1;
    delete TrainC2; //cam2,3 absent in data now
    delete TrainC3;
//    delete PICO60Output; //uncomment this if there is a single writer


    printf("AutoBub done analyzing this run. Thank you.\n");
    return 0;

}
