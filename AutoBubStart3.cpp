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
#include "ParseFolder/Parser.hpp"
#include "ParseFolder/RawParser.hpp"
#include "ParseFolder/ZipParser.hpp"
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

#include <boost/program_options.hpp>

const int evalEntropyThresholdFrames = 2;
std::vector<int> badEvents;


namespace po = boost::program_options;


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
        //AnalyzerCGeneric->ParseAndSortFramesInFolder();
        
        do{
            AnalyzerCGeneric->FindTriggerFrame(nonStopPref,AnalyzerCGeneric->MatTrigFrame+1);
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
                    break;
                    }
            }
            else
            {
                Pico60Writer->stageCameraOutputError(camera,AnalyzerCGeneric->TriggerFrameIdentificationStatus, actualEventNumber);
                break;
            }
        } while (AnalyzerCGeneric->BubbleList.size()==0);

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

std::string usage(){
    std::string msg(
                "Usage: abub3hs [-hz] [-D data_series] [-c cam_mask_dir] -d data_dir -r run_ID -o out_dir\n"
                "Run the AutoBub3hs bubble finding algorithm on a PICO run\n\n"
                "Required arguments:\n"
                "  -d, --data_dir = Dir\t\tpath to the directory in which the run folder/file is stored\n"
                "  -r, --run_id = Str\t\trun ID, formatted as YYYYMMDD_*\n"
                "  -o, --out_dir = Dir\t\tdirectory to write the output file to\n"
                "  -c, --cam_mask_dir = Dir\tdirectory containing the camera mask images. If not included, the mask check is skipped\n\n"
                "Optional arguments:\n"
                "  -h, --help\t\t\tgive this help message\n"
                "  -z, --zip\t\t\tindicate the run is stored as a zip file; otherwise assumed to be in a directory\n"
                "  -D, --data_series = Str\tname of the data series, e.g. 40l-19, 30l-16, etc.\n"
                "  --debug = Int\t\t\t3 digit int; eg: 101: first digit = localizer debug; second digit = multithread off; third digit = analyzer debug\n"
                "  -e, --event = Int\t\tspecify a single event to process. Probably just useful for debugging and testing\n"
    );
    return msg;
}

/*The main autobub code starts here*/
int main(int argc, char** argv)
{
    std::string dataLoc;
    std::string run_number;
    std::string out_dir;
    std::string mask_dir;
    std::string data_series;
    int debug_mode = 0;
    int event_user = -1;
    bool zipped = false;

    // generic options
    po::options_description generic("Arguments");
    generic.add_options()
        ("help,h", "produce help message")
        ("data_series,D", po::value<std::string>(&data_series)->default_value(""), "data series name, e.g. 30l-16, 40l-19, etc.")
        ("zip,z", po::bool_switch(&zipped), "run is stored as a zip file")
        ("data_dir,d", po::value<std::string>(&dataLoc), "directory in which the run is stored")
        ("run_num,r", po::value<std::string>(&run_number), "run ID, formatted as YYYYMMDD_")
        ("out_dir,o", po::value<std::string>(&out_dir), "directory to write the output file to")
        ("cam_mask_dir,c", po::value<std::string>(&mask_dir), "directory containing the camera mask pictures")
        ("debug", po::value<int>(&debug_mode), "debug mode: first digit = localizer debug; second digit = multithread off; third digit = analyzer debug")
        ("event,e", po::value<int>(&event_user), "specify a single event to process")
    ;

    // This part is required for positional arguments.  The call signature is "add(<arg>, <# expected arguments)
    /*
    po::positional_options_description p;
    p.add("data_dir", 1);
    p.add("run_num", 1);
    p.add("out_dir", 1);
    p.add("cam_mask_dir", 1);
    */

    // Parsing arguments
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
            options(generic).run(), vm);
    po::notify(vm);

    if (vm.count("help") | argc == 1){
        std::cout << usage() << std::endl;
        return 1;
    }

    if (dataLoc.compare("") == 0 | run_number.compare("") == 0 | out_dir.compare("") == 0){
        std::cerr << "Insufficient required arguments; use \"autobub3hs -h\" to view required arguments" << std::endl;
        return -1;
    }

    printf("This is AutoBub v3, the automatic unified bubble finder code for all chambers\n");

    std::string this_path = argv[0];
    std::string abub_dir = this_path.substr(0,this_path.find_last_of("/")+1);

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

    if (debug_mode%100/10) omp_set_num_threads(1);

    /*I anticipate the object to become large with many bubbles, so I wanted it on the heap*/
    OutputWriter *PICO60Output = new OutputWriter(out_dir, run_number, frameOffset, numCams);
    PICO60Output->writeHeader();

    /*Construct list of events*/
    std::vector<std::string> EventList;
    int EVstatuscode = 0;

    Parser *FileParser;
    /* The Parser reads directories/zip files and retreives image data */
    if (zipped){
        FileParser = new ZipParser(eventDir, imageFolder, imageFormat);
    }
    else {
        FileParser = new RawParser(eventDir, imageFolder, imageFormat);
    }


    try
    {
        //GetEventDirLists(eventDir.c_str(), EventList, EVstatuscode);
        //if (EVstatuscode != 0) throw "Failed to read directory";
        FileParser->GetEventDirLists(EventList);
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
        Parser *tp = FileParser->clone();
        Trainers.push_back(new Trainer(icam, EventList, eventDir, imageFormat, imageFolder, tp));
    }
    //Trainer *TrainC1 = new Trainer(1, EventList, eventDir, imageFormat, imageFolder);
    //Trainer *TrainC2 = new Trainer(2, EventList, eventDir, imageFormat, imageFolder);
    //Trainer *TrainC3 = new Trainer(3, EventList, eventDir, imageFormat, imageFolder);


    #pragma omp parallel for
    for (int icam = 0; icam < numCams; icam++){
        Trainers[icam]->MakeAvgSigmaImage(false);
    }

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
        
        if (event_user > 0 && evi != event_user) continue;
        
        printf("\rProcessing event: %s / %d  ... ", EventList[evi].c_str(), static_cast<int>(EventList.size())-1);
        advance_cursor(); /*Fancy coursors!*/

        /* ***************************
         * ***** Camera Operations ******
         ********************************/
        //#pragma omp ordered
        std::vector<AnalyzerUnit*> Analyzers;
        for (int icam = 0; icam < numCams; icam++){
            Parser *tp = FileParser->clone();

            Analyzers.push_back(new L3Localizer(EventList[evi], imageDir, icam, debug_mode/100?false:true, &Trainers[icam], mask_dir, tp));
            AnyCamAnalysis(EventList[evi], imageDir, icam, debug_mode%10?false:true, &Trainers[icam], &PICO60Output, out_dir, actualEventNumber, &Analyzers[icam]);
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

    delete FileParser;

    printf("AutoBub done analyzing this run. Thank you.\n");
    return 0;

}
