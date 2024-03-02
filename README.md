# AutoBub

This is AutoBub version 3. 

AutoBub was written from scratch to work with ALL bubble chambers used by PICO. Its purpose is to identify bubbles in images from a bubble chamber and return information about the bubble candidates, including position and movement information. 

The code was originally heavily tested on PICO 30l-16. It needs to be retuned for each new bubble chamber design to insure optimum performance for the conditions of the chamber. 

LBP algorithm has been dropped temporarily in favour of the machine learning and simple profile thresholding which seems to be at par / better than just LBP or machine learning + LBP.

## Installation Instructions

The code is available from the `picoexperiment` project area on Github,

https://github.com/picoexperiment/AutoBub3hs

The usual means to create a personal clone of the software is to use the `git` executable and the `clone` command:

```
git clone https://github.com/picoexperiment/AutoBub3hs.git  
```

You must then compile the software. The following example will work on the Graham computing cluster from AllianceCanada, which is the primary distributed computing platform used by the PICO Collaboration. After logging into Graham and cloning the software, you would do the following from the top-level project folder.

### Load the Environment

Load the following modules to create your build environment:

```
module load gcc boost opencv
```

If you attempt to install this on a system other than Graham you will need to install those software packages and make them available to all software before you build. 

You can also run the provided script designed to setup your environment:

```
source Environment-Graham.sh
```

It accomplishes the same thing on Graham.

### Compile using CMake

The preferred way to build the software using the CMake build system is to first create a folder in the top-level project directory:

```
mkdir build
cd build/
```

This folder is empty and after compiling will contain the libraries and executable(s) produced by CMake. If you make a mistake, you can always delete this folder and start fresh without affecting the project folder's contents (e.g., the original source code).

Now prepare the build environment:

```
cmake ../
```

After this step the following files are created: ```CMakeCache.txt```, ```CMakeFiles/```, ```cmake_install.cmake```, and ```Makefile```. If you need to conduct a fresh build of the software, your can (in principle) minimally just delete these and rerun the ```cmake``` command above. This will pick up any changes that have been made to the source code.

Now compile the software:

```
make -j
```

*(Adding the `-j` option will attempt to use all available cores in the build, which can speed the compiling step)*

The most important output of a successful compile step is the executable called ```abub3hs```. Note also that the build step places a symbolic link to the `cam-masks` folder available from the main AutoBub project. These masks may be needed to more effectively process data, but must be generated for each detector.

We'll learn now how to use this program. 

## Using AutoBub

To display all usage options, run the following: 

```
build$ ./abub3hs --help
```

You might see something like the following:

```
Usage: abub3hs [-hzme] [-D data_series] [-c cam_mask_dir] [--debug code] -d data_dir -r run_ID -o out_dir
Run the AutoBub3hs bubble finding algorithm on a PICO run

Required arguments:
  -d, --data_dir = Dir          path to the directory in which the run folder/file is stored
  -r, --run_id = Str            run ID, formatted as YYYYMMDD_*
  -o, --out_dir = Dir           directory to write the output file to

Optional arguments:
  -h, --help                    give this help message
  -z, --zip                     indicate the run is stored as a zip file; otherwise assumed to be in a directory
  -c, --cam_mask_dir = Dir      directory containing the camera mask images
  -m, --mask_check              use camera masks in default directory. Not needed if directory specified
  -D, --data_series = Str       name of the data series, e.g. 40l-19, 30l-16, etc.
  -e, --event = Int             specify a single event to process. Mostly just useful for debugging and testing
  --debug = Int                 3 digit int; eg: 101: first digit = localizer debug; second digit = multithread off; third digit = analyzer debug
```

AutoBub minimally expects to be given the location of a folder containing raw data, the name of a run that you want to process, the location to write the output, and data series (e.g., 40l-22), and the kind of raw data format (e.g. is it zipped or uncompressed?). For example:


```
build$ ./abub3hs -d /project/rrg-kenclark/pico/40l-22-data/ -r 20240131_0 -o test/ -D 40l-22 -z
```

If you have masks you wish to use during processing, you can tell the program about the `cam-masks` folder:

```
build$ ./abub3hs -d /project/rrg-kenclark/pico/40l-22-data/ -r 20240131_0 -o test/ -c cam-masks/ -D 40l-22 -z
```

### What is AutoBub Doing When I Run It?

It executes the following general steps:

1. It begins by learning ("Learn" mode) about the detector from the images in the run. For example, it uses the images together to identify what the detector looks like when no bubbles are present.
2. It then enters its "Detect" mode where it processes the images to identify bubbles.
3. It writes its output to the directory you specified above.

The program might print messages while running that look like this:

```
This is AutoBub v3, the automatic unified bubble finder code for all chambers
Not performing mask check on this run.
**Starting training. AutoBub is in learn mode**
Camera 0 training ... Camera 2 training ... Camera 3 training ... Camera 1 training ... complete.
complete.
complete.
complete.
***Training complete. AutoBub is now in detect mode***
Total threads: 32
Processing event: 63 / 100  ... /
```

### The Output Format

If you look in your output folder, you will see files with the following format:

```
abub3_<run number>.txt
```

Let's look at the contents of one such file:


```
Output of AutoBub v3 - the automatic unified bubble finder code by Pitam, using OpenCV.
run  ev  ibubimage  TotalBub4CamImg  camera  frame0  hori  vert  GenesisW  GenesisH  dZdt  dRdt  TrkFrame(10)  TrkHori(10)  TrkVert(10)  TrkBubW(10)  TrkBubH(10)  TrkBubRadius(10)  FakeValue
%12s  %5d  %d  %d  %d  %d  %.02f  %.02f  %d  %d  %.02f  %.02f  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %.02f  %d
8


20240131_0  0  1  4  0  61  1740.00  427.00  1  1  0.57  1.15  62  63  64  65  66  67  68  68  69  70  1739.00  1739.15  1738.83  1739.55  1739.55  1740.24  1740.19  -1  -1  -1  426.67  426.80  426.67  426.85  426.85  426.99  426.88  -1  -1  -1  4  5  6  7  7  8  8  -1  -1  -1  3  5  5  5  5  5  5  -1  -1  -1  0.98  1.99  1.95  2.29  2.29  2.29  2.22  -1  -1  -1  1  
20240131_0  0  2  4  1  56  1626.42  1035.93  66  56  -nan  -nan  56  57  58  59  60  61  62  63  64  65  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  1  
20240131_0  0  3  4  2  53  1551.50  767.50  2  4  -6.00  3.16  54  54  55  56  57  58  59  60  61  62  1559.33  -1  -1  -1  -1  -1  -1  -1  -1  -1  768.33  -1  -1  -1  -1  -1  -1  -1  -1  -1  5  -1  -1  -1  -1  -1  -1  -1  -1  -1  3  -1  -1  -1  -1  -1  -1  -1  -1  -1  1.13  -1  -1  -1  -1  -1  -1  -1  -1  -1  1  
20240131_0  0  4  4  3  50  1638.13  911.98  8  8  -nan  -nan  50  51  52  53  54  55  56  57  58  59  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  -1  1  
```

This is hard to read, but the basic idea is that it is a simple column-based data file (in the PICO reconstructed data format). The column names are listed at the top of the file. For each event, and for each detected bubble, four rows (one for each camera) are written to the file. The rows are written first for all the bubbles seen in the camera 0 image, then the bubbles in camera 1, etc. The above example contains just the first event, where one bubble was found and data are reported for each of the four cameras.

Some highlights from the data:

* `hori` and `vert` tell you the location, in pixel space in that image, where the bubble is found in the `frame0` frame.
* `dZdt` and `dRdt` tell you how the location of the "centre-of-mass" of the bubble, and the estimated bubble radius, change with time.
* `Trk*` stores tracking information (e.g. position of the centre-of-mass, bubble radius) for 10 frames of the bubble's evolution.


### Running in Debugging Mode

If you want to assess the performance of AutoBub on data you might need more information. This is obtained by running in debug mode. For example:

```
build$ ./abub3hs -d /project/rrg-kenclark/pico/40l-22-data/ -r 20240131_0 -o test/ -D 40l-22 -z --debug=101
```

Debugging output will be written to two directories, which are created if they don't already exist:

```
DebugPeek/
${HOME}/test/abub_debug/
```

You might want to redirect the output of the AutoBub command to a log file, since it will print a lot of debugging information in this mode. The error codes in the recon file are as follows:

```
-1: Entropy did trigger but no bubble was found  
-2: Entropy was massive in the first frame / usually a late trigger. Analysis not performed.  
-3: Entropy did not trigger on the images of that camera series. (usually if other cameras see a trigger)  
-5: Failed to make a file list of all files in the run (data corruption?)  
-6: Failed to analyze / segfault (malformed images / data corruption /missing images)  
-7: Failed to train on the dataset (some event folders might be empty)  
-10: Unknown storage format (current options are "raw" and "zip")  
```

### What Do I Do With the Output?

Besides looking at the output yourself, you can then pass it to programs like Optometrist and/or XYZLookup to convert the bubble information in pixel space to information in cartesian space. For example, in the final reconstructed data file used by ```PICOcode```, all of that processing has already occurred and the bubble information is stored as coordinated like X, Y, Z, R2, etc.

## How To Submit Issues, Suggestions, and Ideas

Use the built-in issue tracker in Github to submit bug reports, suggestions for new features, etc.


## Acknowledgements

AutoBub was originally developed by Pitam Mitra (University of Alberta, PhD'18). Some useful documentation generated from Pitam's work is as follows:

* Pitam's PhD Thesis: https://www.snolab.ca/pico-docdb/cgi/ShowDocument?docid=3252
* AutoBub Algorithm: https://www.snolab.ca/pico-docdb/cgi/ShowDocument?docid=2031
* AutoBub Image Analysis: https://www.snolab.ca/pico-docdb/cgi/ShowDocument?docid=1578

The following persons have made contributions to the code and/or serve to maintain it:

* Minya Bai (Queen's University)
* Colin Moore (Queen's University)
* Sumanta Pal (University of Alberta)
* Carl Rethmeier (University of Alberta)
* Nolan Sandgathe (University of Alberta)
* Stephen Sekula (Queen's University)

Check the code commits to see who has been most recently active, and ask them for help if you have questions!

https://github.com/picoexperiment/AutoBub3hs/commits/master/


