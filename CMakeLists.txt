cmake_minimum_required(VERSION 3.0)

project(AlviumDriver LANGUAGES CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/alvium-sdk/api/examples/cmake/vmb_cmake_prefix_paths.cmake")
    # read hardcoded package location information, if the example is still located in the original install location
    include(${CMAKE_CURRENT_SOURCE_DIR}/alvium-sdk/api/examples/cmake/vmb_cmake_prefix_paths.cmake)
endif()

find_package(Vmb REQUIRED COMPONENTS CPP ImageTransform NAMES Vmb VmbC VmbCPP VmbImageTransform)

find_package(OpenCV REQUIRED)

add_executable(alvium
    src/main.cpp
    src/Driver.cpp
    include/Driver.h 
    src/Logger.cpp
    include/Logger.h 
    src/Utils.cpp
	include/Utils.h	
)
target_link_libraries(alvium PRIVATE 
	Vmb::CPP
	Vmb::ImageTransform
	${OpenCV_LIBS})

set_target_properties(alvium PROPERTIES
    CXX_STANDARD 17
    VS_DEBUGGER_ENVIRONMENT "PATH=${VMB_BINARY_DIRS};$ENV{PATH}"
)

target_include_directories(alvium PRIVATE 
	${CMAKE_SOURCE_DIR}/include
	${OpenCV_INCLUDE_DIRS}
	${VMB_INCLUDE_DIRS}
)
