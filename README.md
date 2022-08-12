This is AutoBub version 3. It was written from scratch to work with ALL bubble chambers used by PICO. While I have tested this on 30l-16, more testing is needed to assess its performance for the other bubble chambers. LBP algorithm has been dropped temporarily in favour of the machine learning and simple profile thresholding which seems to be at par / better than just LBP or machine learning + LBP.

**Installation:**
git clone https://github.com/picoexperiment/AutoBub3hs.git  

Prior to building on the cluster (i.e., graham), run:
```
module load gcc boost opencv
```
If installing locally, you will need to make sure you have the three packages listed above installed.    

Old way to build: build files are in the source directory

```
cmake .
make
```

Modified way: build files are in a separate build directory

```
mkdir AutoBub3hs-builddir
cd AutoBub3hs-builddir
AutoBub3hs-builddir$ cmake ../AutoBub3hs/
```

Following files are created: CMakeCache.txt, CMakeFiles/, cmake_install.cmake, Makefile. 
Delete these files for a fresh build. 

```
AutoBub3hs-builddir$ make
```

generates executable file 'abub3hs'.

To display full usage: 
```
AutoBub3hs-builddir$ ./abub3hs --help
```
Example usage: 
```
AutoBub3hs-builddir$ ./abub3hs -d /project/rrg-kenclark/pico/40l-19-data/ -r 20200624_1 -o ../test/ -c ./cam_masks/ -D 40l-19 -z
```

Make sure to put the folder `cam_masks` into the same directory as the abub3hs executable (build directory)!

Output:

`abub3_<run number>.txt` in PICO recon format.
You can run in debug mode to see the frames with the recon done on them. Debug frames are created in ./DebugPeek/ and $HOME/test/abub_debug/. These directories need to be created by the user before any images will be saved.

The error codes in the recon file are as follows:

-1: Entropy did trigger but no bubble was found  
-2: Entropy was massive in the first frame / usually a late trigger. Analysis not performed.  
-3: Entropy did not trigger on the images of that camera series. (usually if other cameras see a trigger)  
-5: Failed to make a file list of all files in the run (data corruption?)  
-6: Failed to analyze / segfault (malformed images / data corruption /missing images)  
-7: Failed to train on the dataset (some event folders might be empty)  
-10: Unknown storage format (current options are "raw" and "zip")  



Please use the issue tracker!



