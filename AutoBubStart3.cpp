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
#include "PICOFormatWriter/PICOFormatWriterV4.hpp"
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

    if (argc < 5)
    {
        printf("Not enough parameters.\nUsage: abub3hs <location of data> <run number> <directory for output file> <directory with camera masks> [optional: data_series]\nEg: abub3hs /scratch/$USER/ 20200925_1 /project/rrg-kenclark/$USER/abub_out/ ./cam_masks/ 40l-19\n");
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

    std::string eventDir = dataLoc + "/" + run_number + "/";

    /* The following variables deal with the different ways that images have been
     * saved in different experiments.
     */
    std::string imageFormat;
    std::string imageFolder;
    int frameOffset;
    int numCams;
    if (data_series=="01l-21" | data_series=="2l-16"){
            imageFormat = "cam%dimage %u.bmp";
            imageFolder = "/";
            frameOffset = 0;
            numCams = 2;
    }
    else if (data_series=="40l-19"){
            imageFormat = "cam%d_image%u.png";
            imageFolder = "/Images/";
            frameOffset = 30;
            if (run_number >= "20200713_7"){ numCams = 4; }
            else { numCams = 2; }
    }
    else {
            imageFormat = "cam%d_image%u.png";
            imageFolder = "/Images/";
            frameOffset = 30;
            numCams = 4;
    }

    /*I anticipate the object to become large with many bubbles, so I wanted it on the heap*/
    OutputWriter *PICO60Output = new OutputWriter(out_dir, run_number, frameOffset, numCams);
    PICO60Output->writeHeader();

    /*Construct list of events*/
    std::vector<std::string> EventList;
    int EVstatuscode = 0;

    try
    {
        GetEventDirLists(eventDir.c_str(), EventList, EVstatuscode);
        if (EVstatuscode != 0) throw "Failed to read directory";
    /*Crash handler at the begining of the program - writes -5 if the folder could not be read*/
    }
    catch (...)
    {
        std::cout<<"Failed to read the images from the run. Autobub cannot continue.\n";
        for (int icam = 0; icam < numCams; icam++){ PICO60Output->stageCameraOutputError(icam, -5, -1); }
        PICO60Output->writeCameraOutput();
        return -5;
    }

    /*A sort is unnecessary at this level, but it is good practice and does not cost extra resources*/
    std::sort(EventList.begin(), EventList.end(), eventNameOrderSort);
    /*Event list is now constructed*/




    /*Learn Mode
     *Train on a given set of images for background subtract
     */
    printf("**Starting training. AutoBub is in learn mode**\n");
    std::vector<Trainer*> Trainers;
    for (int icam = 0; icam < numCams; icam++){
        Trainers.push_back(new Trainer(icam, EventList, eventDir, imageFormat, imageFolder));
    }
    //Trainer *TrainC1 = new Trainer(1, EventList, eventDir, imageFormat, imageFolder);
    //Trainer *TrainC2 = new Trainer(2, EventList, eventDir, imageFormat, imageFolder);
    //Trainer *TrainC3 = new Trainer(3, EventList, eventDir, imageFormat, imageFolder);


    //try {
    #pragma omp parallel for
    for (int icam = 0; icam < numCams; icam++){
        Trainers[icam]->MakeAvgSigmaImage(false);
    }
    //} catch (...) {

    /* Check if the trainers succeeded, in which case StatusCode = 0 */
    bool succeeded = true;
    for (int icam = 0; icam < numCams; icam++){
        if (Trainers[icam]->StatusCode){ succeeded = false; }
    }

    if (!succeeded){
        std::cout<<"Failed to train on images from the run. Autobub cannot continue.\n";
        for (int evi=0; evi<EventList.size(); evi++){
            int actualEventNumber = atoi(EventList[evi].c_str());
            for (int icam = 0; icam < numCams; icam++){
                PICO60Output->stageCameraOutputError(icam, -7, actualEventNumber);
            }
            PICO60Output->writeCameraOutput();
        }
        return -7;
    }
    //}


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
        OutputWriter *PICO60Output = new OutputWriter(out_dir, run_number, frameOffset, numCams);
        std::string imageDir=eventDir+EventList[evi]+"/Images/";
        /*We need the actual event number in case folders with events are missing*/
        int actualEventNumber = atoi(EventList[evi].c_str());
//if (evi != 94) continue;
        printf("\rProcessing event: %s / %d  ... ", EventList[evi].c_str(), static_cast<int>(EventList.size())-1);
        advance_cursor(); /*Fancy coursors!*/

        /* ***************************
         * ***** Camera Operations ******
         ********************************/
        //#pragma omp ordered
        std::vector<AnalyzerUnit*> Analyzers;
        for (int icam = 0; icam < numCams; icam++){
            Analyzers.push_back(new L3Localizer(EventList[evi], imageDir, icam, true, &Trainers[icam],mask_dir));
            AnyCamAnalysis(EventList[evi], imageDir, icam, true, &Trainers[icam], &PICO60Output, out_dir, actualEventNumber, &Analyzers[icam]);
        }
        /*
        AnalyzerUnit *AnalyzerC0 = new L3Localizer(EventList[evi], imageDir, 0, true, &TrainC0, mask_dir);
        AnalyzerUnit *AnalyzerC1 = new L3Localizer(EventList[evi], imageDir, 1, true, &TrainC1, mask_dir);
        AnalyzerUnit *AnalyzerC2 = new L3Localizer(EventList[evi], imageDir, 2, true, &TrainC2, mask_dir);
        AnalyzerUnit *AnalyzerC3 = new L3Localizer(EventList[evi], imageDir, 3, true, &TrainC3, mask_dir);

        AnyCamAnalysis(EventList[evi], imageDir, 0, true, &TrainC0, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC0);
        AnyCamAnalysis(EventList[evi], imageDir, 1, true, &TrainC1, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC1);
        AnyCamAnalysis(EventList[evi], imageDir, 2, true, &TrainC2, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC2); //cam 2,3 absent in data now
        AnyCamAnalysis(EventList[evi], imageDir, 3, true, &TrainC3, &PICO60Output, out_dir, actualEventNumber, &AnalyzerC3); //cam 2,3 absent in data now

        */

        /*Write and commit output after each iteration, so in the event of a crash, its not lost*/
        #pragma omp ordered
        {
           PICO60Output->writeCameraOutput();
        }

//        delete AnalyzerC0, AnalyzerC1, AnalyzerC2, AnalyzerC3; //cam 2,3 absent in data now
        delete PICO60Output;
        for (int icam = 0; icam < numCams; icam++) delete Analyzers[icam];
    }

    printf("run complete.\n");

    /*GC*/
    for (int icam = 0; icam < numCams; icam++) delete Trainers[icam];
    //delete TrainC0;
    //delete TrainC1;
    //delete TrainC2; //cam2,3 absent in data now
    //delete TrainC3;
//    delete PICO60Output; //uncomment this if there is a single writer


    printf("AutoBub done analyzing this run. Thank you.\n");
    return 0;

}
