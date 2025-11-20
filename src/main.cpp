/*=============================================================================
  Copyright (C) 2012-2023 Allied Vision Technologies.  All Rights Reserved.
  Subject to the BSD 3-Clause License.
=============================================================================*/

#include "Driver.h"
#include "Logger.h"
#include "Utils.h"

#include <memory>
#include <exception>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <string>
#include <atomic>
#include <csignal>
#include <thread>
#include <condition_variable>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono;

int main(int argc, char* argv[])
{
	SetupSignalHandler();

    std::cout << "////////////////////////////////////////\n";
    std::cout << "//////////// Alvium Driver /////////////\n";
    std::cout << "////////////////////////////////////////\n\n";

    auto now = system_clock::now();
    std::time_t now_time = system_clock::to_time_t(now);

    std::tm tm_local;
    localtime_r(&now_time, &tm_local);
    std::ostringstream oss;
    oss << std::put_time(&tm_local, "%Y-%m-%d_%H%M%S"); 
    fs::path outputDir = "/home/sst/data/alvium_test/" + oss.str();
    fs::path loggerFile = "/home/sst/data/alvium_test/" + oss.str() + "/alvium_log.txt";
    int frameRate = 5;
	bool fixedFlag = false;
    double exposureTime = 100000.0;
	bool exposureFlag = false;
    std::string mode = "fixed";
    bool processing = false;
    bool debug = false;
    bool timing = false;
	bool running = true;
    int core = -1;
    VmbCPP::Examples::ROI roi;
	
    for (int i = 1; i < argc; ++i) 
    {
	    std::string arg = argv[i];

	    if (arg == "--output" && i + 1 < argc)
	    {
	    	outputDir = argv[++i];
			loggerFile = outputDir / "alvium_log.txt";
    	}
	    else if (arg == "--framerate" && i + 1 < argc)
	    {
		    frameRate = std::stoi(argv[++i]);
			fixedFlag = true;
		    if (frameRate < 0 || frameRate > 30)
		    {
			    std::cerr << "Frame rate must be between 0 and 30.\n";
			    return 1;
		    }
	    }
	    else if (arg == "--mode" && i + 1 < argc)
	    {
		    mode = argv[++i];
		    if (mode != "fixed" && mode != "trigger" && mode != "trigger_keyboard" && mode != "exposure")
		    {
			    std::cerr << "Invalid mode. Use 'fixed', 'trigger', 'trigger_keyboard' or 'exposure'.\n";
			    return 1;
		    }
	    }
	    else if (arg == "--exposure" && i + 1 < argc)
	    {
		    exposureTime = std::stod(argv[++i]);
			exposureFlag = true;
	    }

	    else if (arg == "--processing")
	    {
		    processing = true;
	    }
	    else if (arg == "--debug")
	    {
		    debug = true;
	    }	
	    else if (arg == "--timing")
	    {
		   timing = true;
	      } 
	    else if (arg == "--core" && i + 1 < argc)
	    {
		    core = std::stoi(argv[++i]);
		    if (core < 0 || core > 3)
		    {
			    std::cerr << "Core ID must be between 0 and 3.\n";
			    return 1;
		    }
	    }
        else if (arg == "--roi" && i + 1 < argc)
        {
            std::string roiStr = argv[++i];
            if (roiStr == "1/4") {
                roi.width = 2064;
                roi.height = 1504;
                roi.offsetX = 1040;
                roi.offsetY = 752;
            }
            else if (roiStr == "1/16") {
                roi.width = 1032;
                roi.height = 752;
                roi.offsetX = 1552;
                roi.offsetY = 1128;
            }
            else {
                auto roi_params = split(roiStr, ',');

                if (roi_params.size() == 4)
                {
                    roi.width = std::stoi(roi_params[0]);
                    roi.height = std::stoi(roi_params[1]);
                    roi.offsetX = std::stoi(roi_params[2]);
                    roi.offsetY = std::stoi(roi_params[3]);
                    if ((roi.width < 0) || (roi.height < 0) || (roi.offsetX < 0) || (roi.offsetY < 0) || (roi.width > 4128) || (roi.height > 3008) || (roi.offsetX > 4128) || (roi.offsetY > 3008) || (roi.width + roi.offsetX > 4128) || (roi.height + roi.offsetY > 3008)) {
                        std::cerr << "Requested ROI dimensions exceed allowable image space. Please choose dimensions and offset such that width is less than 4128 and height is less than 3008. \n";
                    }
                }
                else
                {
                    std::cerr << "Invalid ROI format. Use: --roi width,height,offsetX,offsetY\n";
                    return 1;
                }
            }
        }

	    else if (arg == "--help")
	    {
		    std::cout << "alvium 0.1.0" << std::endl;
		    std::cout << "Adyn Miles (adyn.miles@starspectech.com)" << std::endl;
		    std::cout << "Runs the Alvium U-1242 C camera." << std::endl;
		    std::cout << "USAGE:" << std::endl;
		    std::cout << "	--help		Prints help information" << std::endl << std::endl;
		    std::cout << "OPTIONS" << std::endl;
		    std::cout << "	--output	Directory to save images" << std::endl;
		    std::cout << "	--framerate	Desired frame rate (0 - 30 Hz)" << std::endl;
		    std::cout << "	--exposure	Desired exposure time (64 - 10000000 us)" << std::endl;
		    std::cout << "	--mode		Choose between fixed frame rate, triggered, and fixed exposure time operation" << std::endl;
		    std::cout << "	--processing 	Choose whether to save .raw images or .png images" << std::endl;
		    std::cout << "	--debug		Choose to log DEBUG information" << std::endl;
		    std::cout << "	--timing	Choose to log only frame timing information" << std::endl;
		    std::cout << "	--core		Core to lock camera process to" << std::endl;
            std::cout << "  --roi       Choose region of interest (use '1/4' for quarter image, '1/16' for one-sixteenth image, or add a custom width, height, offsetX, and offsetY" << std::endl;
            return 1;
	    }
	    else {
		    std::cerr << "Unknown argument: " << arg << "\n";
		    std::cerr << "Usage: " << argv[0]
			      << " [--output <directory>] [--framerate <0-30>] [--exposure <64 - 10000000>] [--mode <fixed/trigger/trigger_keyboard/exposure>] [--processing] [--debug] [--timing] [--core <0-3>] [--roi <width,height,offsetX,offsetY>] \n";
		    return 1;
	    }

    }

	if ((mode != "exposure") && (exposureFlag == true)) {
		std::cerr << "Cannot input custom exposure time when not in exposure mode. Set with --mode 'exposure'." << std::endl;
	}

	if ((mode != "fixed") && (mode != "trigger") && (fixedFlag == true)) {
		std::cerr << "Cannot input fixed frame rate when not in fixed frame rate or trigger mode. Set with --mode 'fixed'." << std::endl; 
	} 

    if (!fs::exists(outputDir))
    {
	    try
	    {
		    fs::create_directories(outputDir);
	    }
	    catch (const std::exception& e)
	    {
		    std::cerr << "Failed to create directory: " << outputDir
			      << "\nReason: " << e.what() << std::endl;
		    return 1;
	    }
    }

    std::shared_ptr<Logger> logger = std::make_shared<Logger>(
		    loggerFile,
		    debug,
		    5000);

    std::cout << "Saving images to: " << outputDir << std::endl;
    std::cout << "Camera frame rate: " << frameRate << " fps" << std::endl;
    std::cout << "Image processing set to " << std::boolalpha <<  processing << std::endl;
	std::cout << "Running camera on core " << core << std::endl;
	
    try
    {
        VmbCPP::Examples::Driver Driver(nullptr, outputDir, logger, frameRate, mode, exposureTime, processing, timing, core, roi);
		
		Driver.Start();
		initTermios();
		
        if ((mode == "fixed") || (mode == "exposure")) {
				std::cout << "Press <enter> to stop acquisition" << std::endl;
				while(running) {
					if (StopRequested()) {
						running = false;
						logger->log("Interrupt signal detected. Shutting down.");
						std::cout << "Shutting down..." << std::endl;
						break;
					}
					
					char c;
					c = getch();
					if (c == '\n') {
						running = false;
						logger->log("Exit key pressed. Shutting down.");
						std::cout << "Shutting down..." << std::endl;
						break;
					}
			  }
		}
		else if (mode == "trigger_keyboard") {
			std::cout << "Press <F> to trigger a frame capture. Press <enter> to quit." << std::endl;
			while(running) {
				if (StopRequested()) {
					running = false;
					logger->log("Interrupt signal detected. Shutting down.");
					std::cout << "Shutting down..." << std::endl;
					break;
				}

				char c;
				c = getch();
				if (c == 'f' || c == 'F') {
					Driver.TriggerFrame();
				}
				else if (c == '\n') {
					running = false;
					logger->log("Exit key pressed. Shutting down.");
					std::cout << "Shutting down..." << std::endl;
					break;
				}
			}
		}
        else if (mode == "trigger") {
            duration<double> interval((1.0 / frameRate) - 0.001);
            auto start_time = std::chrono::steady_clock::now();
            auto next_trigger = start_time + interval;

            std::cout << "Press <enter> to stop acquisition." << std::endl;
            while (running) {
                if (StopRequested()) {
                    running = false;
                    logger->log("Interrupt signal detected. Shutting down.");
                    std::cout << "Shutting down..." << std::endl;
                    break;
                }
                char c;
                c = getch();
                if (c == '\n') {
                    running = false;
                    logger->log("Exit key pressed. Shutting down.");
                    std::cout << "Shutting down..." << std::endl;
                    break;
                }
                auto now = std::chrono::steady_clock::now();
                if (now >=  next_trigger) {
                    Driver.TriggerFrame();

                    next_trigger += interval;
                    
                    if (now > next_trigger) {
                        next_trigger = now + interval;
                    }
                }

            }
        }

		resetTermios();
    }
    catch (std::runtime_error& e)
    {
        logger->error(std::string("Error: ") + e.what());
        return 1;
    }

    return 0;
    // AcquisitionHelper's destructor will stop the acquisition and shutdown the VmbCPP API
}
