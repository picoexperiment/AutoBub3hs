cmake_minimum_required(VERSION 2.8)
project( DisplayImage )

find_package( OpenCV REQUIRED )
find_package( OpenMP REQUIRED )
find_package( Boost REQUIRED COMPONENTS regex filesystem program_options QUIET )
execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Minizip-ng options override - Only compile Zlib support
set(MZ_COMPAT OFF CACHE BOOL "Enable compatibility layer with minizip
(non-ng)")
set(MZ_BZIP2 OFF CACHE BOOL "Compile bzip2 support")
set(MZ_LIBBSD OFF CACHE BOOL "Compile libbsd support")
set(MZ_LZMA OFF CACHE BOOL "Compile LZMA support")
set(MZ_OPENSSL OFF CACHE BOOL "Enable OpenSSL support for encryption")
set(MZ_PKCRYPT OFF CACHE BOOL "Enable PKWARE support for encryption")
set(MZ_SIGNING OFF CACHE BOOL "Enable zip signing")
set(MZ_WZAES OFF CACHE BOOL "Enable WinZIP AES encryption")
set(MZ_ZSTD OFF CACHE BOOL "Enable ZSTD compression")

add_subdirectory(minizip-ng)

# Required for minizip-ng
find_package(ZLIB QUIET)

set(CMAKE_MODULE_PATH "cmake_modules")
#find_package(sqlite3 REQUIRED)
#MESSAGE( STATUS "Found : ${SQLITE3_LIBRARIES}" )

#Compile against ROOT. Currently just using TMath.h to calculate Chi2 prob.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} .)   #Use FindROOT.cmake
find_package(ROOT)
set(CMAKE_EXE_LINKER_FLAGS "-Wno-write-string -I${ROOT_INCLUDE_DIR}  ${ROOT_LIBRARIES}")

#SET(CMAKE_CXX_FLAGS "-std=gnu++1z -O3 -march=native -m64 ${OpenMP_CXX_FLAGS} -I${OpenCV_INCLUDE_DIRS}/")
SET(CMAKE_CXX_FLAGS "-std=gnu++1z -O0 -march=native -m64 ${OpenMP_CXX_FLAGS} -I${OpenCV_INCLUDE_DIRS}/ -ggdb")
#SET(CMAKE_CXX_FLAGS "-std=gnu++1z -O0 -march=native -m64 ${OpenMP_CXX_FLAGS} -ggdb")

file(GLOB LBP_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/LBP/*.hpp)
file(GLOB LBP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/LBP/*.cpp)

add_library(lbpuser SHARED ${LBP_SOURCES})
target_link_libraries(lbpuser ${OpenCV_LIBS} ${ROOT_LIBRARIES})
set_property(TARGET lbpuser PROPERTY PUBLIC_HEADER ${LBP_HEADERS})



#Entropy analysis subsystem
set(EntropySources
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageEntropyMethods/ImageEntropyMethods.cpp
)
set(EntropyHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageEntropyMethods/ImageEntropyMethods.hpp
)
add_library(AbubEntropySubsystem SHARED ${EntropySources})
target_link_libraries(AbubEntropySubsystem ${OpenCV_LIBS})
set_property(TARGET AbubEntropySubsystem PROPERTY PUBLIC_HEADER ${EntropyHeaders})


#utility and folder parsing
set(UtilitySources
    ${CMAKE_CURRENT_SOURCE_DIR}/common/UtilityFunctions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/ParseFolder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/Parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/RawParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/ZipParser.cpp
)
set(UtilityHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/common/UtilityFunctions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/ParseFolder.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/Parser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/RawParser.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/ZipParser.hpp
)
add_library(AbubUtility SHARED ${UtilitySources})
target_include_directories(AbubUtility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/minizip-ng/)

target_link_libraries(AbubUtility ${OpenCV_LIBS} ${Boost_LIBRARIES}
    ${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${LIBLZMA_LIBRARIES}
    ${ZSTD_LIBRARIES} ${OPENSSL_LIBRARIES})
set_property(TARGET AbubUtility PROPERTY PUBLIC_HEADER ${UtilityHeaders})



set(SOURCE
   ${CMAKE_CURRENT_SOURCE_DIR}/BubbleLocalizer/L3Localizer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AlgorithmTraining/Trainer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AnalyzerUnit.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/PICOFormatWriter/PICOFormatWriterV4.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bubble/bubble.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/FrameSorter.cpp
)
set(HEADERS
   ${CMAKE_CURRENT_SOURCE_DIR}/common/CommonDatatypes.h
   ${CMAKE_CURRENT_SOURCE_DIR}/BubbleLocalizer/L3Localizer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AlgorithmTraining/Trainer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AnalyzerUnit.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/PICOFormatWriter/PICOFormatWriterV3.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bubble/bubble.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/FrameSorter.hpp
)

add_executable( abub3hs AutoBubStart3.cpp )
target_include_directories(abub3hs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/minizip-ng/)
target_link_libraries( abub3hs ${OpenCV_LIBS} ${ROOT_LIBRARIES} MathMore ${SOURCE} ${HEADERS} lbpuser
    AbubEntropySubsystem AbubUtility minizip)
#target_link_libraries( abub ${OpenCV_LIBS} ${SOURCE} ${HEADERS} )
