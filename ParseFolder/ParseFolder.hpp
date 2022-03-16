/* *****************************************************************************
 * This file contains the necessary functions for Parsing an event folder
 * ALGORITHM given a folder, finds the .jpg files and sorts them by time.
 *
 * by Pitam Mitra for the PICO Geyser ImageAnalysis Algorithm.
 *
 * Created: 1 Feb 2014
 *
 * Issues: None atm
 *
 *******************************************************************************/
#include <vector>
#include <string>

#ifndef ParseFolder_HPP_INCLUDED
#define ParseFolder_HPP_INCLUDED

/*Decalarions*/

void GetFileLists(const char*, std::vector<std::string>& , int*, const char* );
void GetEventDirLists(const char*, std::vector<std::string>& , int& );

#endif
