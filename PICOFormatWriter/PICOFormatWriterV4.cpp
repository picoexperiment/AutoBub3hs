#include <vector>
#include <string>
#include <dirent.h>
#include <iostream>


#include <opencv2/opencv.hpp>
#include "PICOFormatWriterV4.hpp"
#include "../bubble/bubble.hpp"
#include "../common/CommonParameters.h"



//#include "ImageEntropyMethods/ImageEntropyMethods.hpp"
//#include "LBP/LBPUser.hpp"




OutputWriter::OutputWriter(std::string OutDir, std::string run_number, int frameOffset, int NumCams)
{
    /*Give the properties required to make the object - the identifiers i.e. the camera number, and location*/
    this->OutputDir = OutDir;
    this->run_number = run_number;

    this->abubOutFilename = this->OutputDir+"abub3hs_"+this->run_number+".txt";
    //this->OutFile.open(this->abubOutFilename);

    this->frameOffset = frameOffset;
    this->NumCams = NumCams;

    for (int i=0; i < this->NumCams; i++){
        this->AllBubbleData.push_back(new BubbleData());
    }

}

OutputWriter::~OutputWriter(void ) {
    for (int i=0; i < this->NumCams; i++){
        delete this->AllBubbleData[i];
    }
}



/*! \brief Function writes headers for the output file
 *
 * \param void
 * \return void
 *
 * This function writes the header file for abub3 output
 */

void OutputWriter::writeHeader(void ){

    this->OutFile.open(this->abubOutFilename);
    this->OutFile<<"Output of AutoBub v3 - the automatic unified bubble finder code by Pitam, using OpenCV.\n";
    this->OutFile<<"run  ev  ibubimage  TotalBub4CamImg  camera  frame0  hori  vert  GenesisW  GenesisH  dZdt  dRdt  ";
        this->OutFile<<"TrkFrame("<<NumFramesBubbleTrack<<")  TrkHori("<<NumFramesBubbleTrack<<")  TrkVert("<<NumFramesBubbleTrack<<")  ";
        this->OutFile<<"TrkBubW("<<NumFramesBubbleTrack<<")  TrkBubH("<<NumFramesBubbleTrack<<")  TrkBubRadius("<<NumFramesBubbleTrack<<")  FakeValue\n";
    this->OutFile<<"%12s  %5d  %d  %d  %d  %d  %.02f  %.02f  %d  %d  %.02f  %.02f  ";

        /*Tracking data*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->OutFile<<"%d "<<" ";

        /*Bubble Position*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->OutFile<<"%.02f "<<" ";

        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->OutFile<<"%.02f "<<" ";

        /*Bubble size*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->OutFile<<"%.02f "<<" ";

        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->OutFile<<"%.02f "<<" ";

        /*Radius*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->OutFile<<"%.02f "<<" ";

    this->OutFile<<"%d";
    this->OutFile<<"\n8\n\n\n";
    this->OutFile.close();
}

/*! \brief Function stages and sorts the Rects as they come from the bubble identifier if there was no error int he process
 *
 * \param std::vector<cv::Rect> BubbleData
 * \param int camera
 * \return void
 *
 * This function writes the header file for abub3 output
 */

void OutputWriter::stageCameraOutput(std::vector<bubble*> BubbleRectIn, int camera, int frame0, int event){

    int tempStatus;
    if (BubbleRectIn.size()==0) tempStatus = -1;    //Error code -1
    else tempStatus = 0;

    this->AllBubbleData[camera]->BubbleObjectData = BubbleRectIn;
    this->AllBubbleData[camera]->StatusCode = tempStatus;
    this->AllBubbleData[camera]->frame0 = frame0;
    this->AllBubbleData[camera]->event = event;

}


/*! \brief Function stages error
 *
 * \param std::vector<cv::Rect> BubbleData
 * \param int camera
 * \return void
 *
 * This function writes the header file for abub3 output
 */


void OutputWriter::stageCameraOutputError(int camera, int error, int event){

    this->AllBubbleData[camera]->StatusCode = error;
    this->AllBubbleData[camera]->event = event;
}

OutputWriter::BubbleData::BubbleData(){};


void OutputWriter::formEachBubbleOutput(int camera, int &ibubImageStart, int nBubTotal){

    this->_StreamOutput.clear();
    this->_StreamOutput.precision(2);
    this->_StreamOutput.setf(std::ios::fixed, std::ios::floatfield);


    BubbleData *workingData = this->AllBubbleData[camera];


    //int event;
    //int frame0=50;
    //run ev ibubimage TotalBub4CamImg camera frame0 GenesisX GenesisY GenesisW GenesisH dZdt dRdt\n";

    if (workingData->StatusCode !=0) {
        this->_StreamOutput<<this->run_number<<"  "<<workingData->event<<"  "<<0<<"  "<<0<<"  "<<camera<<"  "<<workingData->StatusCode<<"  "<<0.0<<"  "<<0.0<<"  "<<0<<"  "<<0;
        this->_StreamOutput<<"  "<<0.0<<"  "<<0.0<<"  ";

        /*Tracking data*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->_StreamOutput<<0<<"  ";

        /*Position*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->_StreamOutput<<0.0<<"  ";

        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->_StreamOutput<<0.0<<"  ";

        /*Bubble Size*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->_StreamOutput<<0.0<<"  ";

        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->_StreamOutput<<0.0<<"  ";

        /*Radius*/
        for (int j=1; j<=NumFramesBubbleTrack; j++)
            this->_StreamOutput<<0.0<<"  ";



        this->_StreamOutput<<"1  \n";



    } else {
    /*Write all outputs here*/
        for (int i=0; i<workingData->BubbleObjectData.size(); i++){
            //run ev iBubImage TotalBub4CamImage camera
            this->_StreamOutput<<this->run_number<<"  "<<workingData->event<<"  "<<ibubImageStart+i<<"  "<<nBubTotal<<"  "<<camera<<"  ";
            //frame0
            this->_StreamOutput<<workingData->frame0 + this->frameOffset << "  ";
            //hori vert smajdiam smindiam


            float width=workingData->BubbleObjectData[i]->GenesisPosition.width;
            float height=workingData->BubbleObjectData[i]->GenesisPosition.height;


            float x = (float)workingData->BubbleObjectData[i]->GenesisPositionCentroid.x; //+width/2.0;
            float y = (float)workingData->BubbleObjectData[i]->GenesisPositionCentroid.y;// +height/2.0;

            float dzdt = workingData->BubbleObjectData[i]->dZdT();
            float drdt = workingData->BubbleObjectData[i]->dRdT();

            int numPointsTracked =  workingData->BubbleObjectData[i]->KnownDescriptors.size()-1;



            int numPointsExcess = NumFramesBubbleTrack > numPointsTracked ? NumFramesBubbleTrack-numPointsTracked : 0;



            this->_StreamOutput<<x<<"  "<<y<<"  "<<(int)width<<"  "<<(int)height<<"  "<<dzdt<<"  "<<drdt<<"  ";

            /*Tracking data*/
            if (numPointsExcess <= 0 ){
                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->frame0 + this->frameOffset + j <<"  ";

                /*Tracking*/
                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].MassCentres.x<<"  ";

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].MassCentres.y<<"  ";


                /*Bubble size*/
                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].newPosition.width<<"  ";

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].newPosition.height<<"  ";

                /*Radius*/
                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].ContRadius<<"  ";



            } else if (numPointsExcess > 0 ){

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->frame0+this->frameOffset+j<<"  ";
                for (int j=0; j<numPointsExcess; j++)
                    this->_StreamOutput<<workingData->frame0+this->frameOffset+numPointsTracked+j<<"  ";

                /*Tracking*/

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].MassCentres.x<<"  ";
                for (int j=0; j<numPointsExcess; j++)
                    this->_StreamOutput<<-1<<"  ";

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].MassCentres.y<<"  ";
                for (int j=0; j<numPointsExcess; j++)
                    this->_StreamOutput<<-1<<"  ";

                /*Bubble size*/

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].newPosition.width<<"  ";
                for (int j=0; j<numPointsExcess; j++)
                    this->_StreamOutput<<-1<<"  ";

                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].newPosition.height<<"  ";
                for (int j=0; j<numPointsExcess; j++)
                    this->_StreamOutput<<-1<<"  ";

                /*Radius*/
                for (int j=1; j<=numPointsTracked; j++)
                    this->_StreamOutput<<workingData->BubbleObjectData[i]->KnownDescriptors[j].ContRadius<<"  ";
                for (int j=0; j<numPointsExcess; j++)
                    this->_StreamOutput<<-1<<"  ";


            }

            this->_StreamOutput<<"1  \n";

        }

        ibubImageStart += workingData->BubbleObjectData.size();

    }


}



void OutputWriter::writeCameraOutput(void){

    int ibubImageStart = 1;

    int nBubTotal = 0;
    for (int i = 0; i < this->NumCams; i++){
        nBubTotal += this->AllBubbleData[i]->StatusCode!=0 ? 0 :  this->AllBubbleData[i]->BubbleObjectData.size();
    }


    for (int i = 0; i < this->NumCams; i++){ this->formEachBubbleOutput(i, ibubImageStart, nBubTotal); }

    this->OutFile.open(this->abubOutFilename, std::fstream::out | std::fstream::app);
    this->OutFile<<this->_StreamOutput.rdbuf();
    this->OutFile.close();
}


