cmake_minimum_required(VERSION 2.8)
project( DisplayImage )
find_package( OpenCV REQUIRED )
find_package( OpenMP REQUIRED )

set(CMAKE_MODULE_PATH "cmake_modules")
#find_package(sqlite3 REQUIRED)
#MESSAGE( STATUS "Found : ${SQLITE3_LIBRARIES}" )


SET(CMAKE_CXX_FLAGS "-std=gnu++1z -Og -march=native -m64 ${OpenMP_CXX_FLAGS} -I${OpenCV_INCLUDE_DIRS}/")

file(GLOB LBP_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/LBP/*.hpp)
file(GLOB LBP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/LBP/*.cpp)

add_library(lbpuser SHARED ${LBP_SOURCES})
target_link_libraries(lbpuser ${OpenCV_LIBS})
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
)
set(UtilityHeaders
    ${CMAKE_CURRENT_SOURCE_DIR}/common/UtilityFunctions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ParseFolder/ParseFolder.hpp
)
add_library(AbubUtility SHARED ${UtilitySources})
target_link_libraries(AbubUtility ${OpenCV_LIBS})
set_property(TARGET AbubUtility PROPERTY PUBLIC_HEADER ${UtilityHeaders})



set(SOURCE
   ${CMAKE_CURRENT_SOURCE_DIR}/BubbleLocalizer/L3Localizer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AlgorithmTraining/Trainer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AnalyzerUnit.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/PICOFormatWriter/PICOFormatWriterV3.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bubble/bubble.cpp
)
set(HEADERS
   ${CMAKE_CURRENT_SOURCE_DIR}/common/CommonDatatypes.h
   ${CMAKE_CURRENT_SOURCE_DIR}/BubbleLocalizer/L3Localizer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AlgorithmTraining/Trainer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/AnalyzerUnit.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/PICOFormatWriter/PICOFormatWriterV3.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bubble/bubble.hpp
)



add_executable( abub3hs AutoBubStart3.cpp )
target_link_libraries( abub3hs ${OpenCV_LIBS} ${SOURCE} ${HEADERS} lbpuser AbubEntropySubsystem AbubUtility )
#target_link_libraries( abub ${OpenCV_LIBS} ${SOURCE} ${HEADERS} )
