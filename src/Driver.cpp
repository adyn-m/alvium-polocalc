/*=============================================================================
  Copyright (C) 2012-2023 Allied Vision Technologies. All Rights Reserved.
  Subject to the BSD 3-Clause License.
=============================================================================*/

#include "Driver.h"
#include "Logger.h"
#include "Utils.h"

#include <VmbCPP/VmbCPP.h>

#include <opencv2/opencv.hpp>
#include <VmbImageTransform/VmbTransform.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <atomic>
#include <memory>
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <condition_variable>

namespace VmbCPP {
namespace Examples {
				

class FrameObserver : public IFrameObserver
{
    public:
        FrameObserver(CameraPtr camera, const std::string& saveDir, std::shared_ptr<::Logger> logger, std::shared_ptr<FrameQueue> queue) : IFrameObserver(camera), m_saveDir(saveDir),  m_queue(queue), m_logger(logger), m_frameCounter(1)  {}

        void FrameReceived(const FramePtr frame) override
        {
            VmbFrameStatusType status;
            if (frame->GetReceiveStatus(status) == VmbErrorSuccess
                    && status == VmbFrameStatusComplete)
            {
                std::ostringstream oss; 
                oss << m_saveDir << "/frame_" << std::setw(6) << std::setfill('0') << m_frameCounter++ << ".raw";
                m_logger->log(oss.str() + " captured.");
                m_queue->push(frame);
            }

            m_pCamera->QueueFrame(frame);

        }
    private:
        std::shared_ptr<FrameQueue> m_queue;
        std::shared_ptr<::Logger> m_logger;
        std::string m_saveDir;
        std::atomic<int> m_frameCounter;

};

void FrameQueue::push(const FramePtr& f) 

{
    std::lock_guard<std::mutex> lock(mtx);
    q.push(f);
    cv.notify_one();
}

FramePtr FrameQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]{ return !q.empty(); });
    FramePtr f = q.front();
    q.pop();
    return f;
}


// Helper function to adjust the packet size for Allied vision GigE cameras
void GigEAdjustPacketSize(CameraPtr camera, std::shared_ptr<::Logger> m_logger)
{
    StreamPtrVector streams;
    VmbErrorType err = camera->GetStreams(streams);

    if (err != VmbErrorSuccess || streams.empty())
    {
        m_logger->error("Could not get stream modules, err=" + std::to_string(err));
		throw std::runtime_error("Could not get stream modules, err=" + std::to_string(err));
    }

    FeaturePtr feature;
    err = streams[0]->GetFeatureByName("GVSPAdjustPacketSize", feature);

    if (err == VmbErrorSuccess)
    {
        err = feature->RunCommand();
        if (err == VmbErrorSuccess)
        {
            bool commandDone = false;
            do
            {
                if (feature->IsCommandDone(commandDone) != VmbErrorSuccess)
                {
                    break;
                }
            } while (commandDone == false);
        }
        else
        {
			m_logger->error("Error while executing GVSPAdjustPacketSize, err=" + std::to_string(err));
        }
    }
}


// Main driver constructor to accept camera configurations and initialize for acquisition
Driver::Driver(const char* cameraId, const std::string& saveDirectory, std::shared_ptr<::Logger> logger, int frameRate, const std::string& mode, int exposureTime, bool processing, bool timing, int core_id, const ROI& roi) :
    m_vmbSystem(VmbSystem::GetInstance()), m_saveDir(saveDirectory), m_logger(logger), m_frameRate(frameRate), m_mode(mode), m_exposureTime(exposureTime), m_processing(processing), m_timing(timing), m_coreid(core_id), m_roi(roi)
{
	// Attempt to access the VmbCPP API
    VmbErrorType err = m_vmbSystem.Startup();

    if (err != VmbErrorSuccess)
    {
		m_logger->error("Could not start API, err=" + std::to_string(err));
        throw std::runtime_error("Could not start API, err=" + std::to_string(err));
    }

	// Attempt to open any connected cameras
    CameraPtrVector cameras;
    err = m_vmbSystem.GetCameras(cameras);
    if (err != VmbErrorSuccess)
    {
        m_vmbSystem.Shutdown();
		m_logger->error("Could not get cameras, err=" + std::to_string(err));
        throw std::runtime_error("Could not get cameras, err=" + std::to_string(err));
    }

    if (cameras.empty())
    {
        m_vmbSystem.Shutdown();
		m_logger->error("No cameras found.");
        throw std::runtime_error("No cameras found.");
    }
	
    if (cameraId != nullptr)
    {
        err = m_vmbSystem.GetCameraByID(cameraId, m_camera);
        if (err != VmbErrorSuccess)
        {
            m_vmbSystem.Shutdown();
			m_logger->error("No camera found with ID=" + std::string(cameraId) + ", err = " + std::to_string(err));
            throw std::runtime_error("No camera found with ID=" + std::string(cameraId) + ", err = " + std::to_string(err));
        }
    }
    else
    {
        m_camera = cameras[0];
    }
	// Attempt to open discovered camera
    err = m_camera->Open(VmbAccessModeFull);
    if (err != VmbErrorSuccess)
    {
        m_vmbSystem.Shutdown();
        throw std::runtime_error("Could not open camera, err=" + std::to_string(err));
    }


	// Log successful camera initialization
    std::string name;
    if (m_camera->GetName(name) == VmbErrorSuccess)
    {
		if (!m_timing) {
			m_logger->log("Opened camera " + name + " successfully");
		}
	}

    try
    {
        GigEAdjustPacketSize(m_camera, m_logger);
    }
    catch (std::runtime_error& e)
    {
        m_vmbSystem.Shutdown();
        throw e;
    }

	// Set fixed frame rate, fixed exposure, or triggered frame mode based on --mode input argument
    if (m_mode == "fixed")
    {
	    ConfigureFixedFrameRate();
    }
    else if ((m_mode == "trigger_keyboard") || (m_mode == "trigger"))
    {
	    ConfigureTriggerMode();
    }
	else if (m_mode == "exposure")
	{
		ConfigureExposureMode();
	}


	// Set core locking affinity based on --core_id input argument
    if (m_coreid != -1) {
	    SetCpuAffinity();
    }

    SetROI();


}


// Driver class destructor that shuts down camera when shutdown key is pressed.
Driver::~Driver()
{
    std::string name;
    m_camera->GetName(name);
    try
    {
        Stop();
    }
    catch(std::runtime_error& e)
    {
		m_logger->error("Could not close camera successfully.");
        std::cout << e.what() << std::endl;
    }
    catch (...)
    {
        // ignore
    }

    if (!m_timing) 
	{
	    m_logger->log("Closed camera " + name + " successfully");
    }

	m_logger->save();
    m_vmbSystem.Shutdown();
}

// Method for starting asynchronous camera acquisition
void Driver::Start()
{
    m_queue = std::make_shared<FrameQueue>();

    m_running = true;
    m_workerThread = std::thread(
            &Driver::FrameWorkerLoop, this
            );

    VmbErrorType err = m_camera->StartContinuousImageAcquisition(5, IFrameObserverPtr(new FrameObserver(m_camera, m_saveDir, m_logger, m_queue)));
    if (!m_timing) {
    	m_logger->log("Started image acquisition.");
    }
    if (err != VmbErrorSuccess)
    {
		m_logger->error("Could not start acquisition, err=" + std::to_string(err));
        throw std::runtime_error("Could not start acquisition, err=" + std::to_string(err));
    }
}

// Method to listen for software triggers and acquire the frame accordingly.
void Driver::TriggerFrame()
{
    if (m_mode == "trigger_keyboard") {
        std::cout << "Frame triggered" << std::endl;
    }
	FeaturePtr triggerCmd;
	m_camera->GetFeatureByName("TriggerSoftware", triggerCmd);
	if (triggerCmd->RunCommand() == VmbErrorSuccess) {
		m_logger->debug("Triggered Image Acquisition.");
	}
}

void Driver::FrameWorkerLoop()
{
    uint64_t frameCounter = 0;
    while (m_running) {
        FramePtr frame = m_queue->pop();
        if (!frame) {
            continue;
        }
        
        frameCounter++;

        unsigned char* buffer = nullptr;
        frame->GetImage(buffer);

        VmbUint32_t width=0, height=0;
        frame->GetWidth(width);
        frame->GetHeight(height);

        if(!m_processing) {
            std::ostringstream oss;
            oss << m_saveDir << "/frame_" << std::setw(6) << std::setfill('0') << frameCounter << ".raw";

            std::ofstream out(oss.str(), std::ios::out | std::ios::binary);
            out.write(reinterpret_cast<char*>(buffer), width * height * 3);
            if (!m_timing) {
                m_logger->debug(oss.str() + " saved.");
            }
        }
        else {
            VmbPixelFormatType pixelFormat;
            VmbError_t convErr;
            VmbErrorType formatResult = frame->GetPixelFormat(pixelFormat);
            VmbImage dstImg = {};
            dstImg.Size = sizeof(VmbImage);

            convErr = VmbSetImageInfoFromPixelFormat(pixelFormat, width, height, &dstImg);
            
            dstImg.Data = buffer;
            frame->GetBufferSize(dstImg.Size);

            cv::Mat output_img(height, width, CV_8UC3, dstImg.Data);
            std::ostringstream oss;
            oss << m_saveDir << "/frame_" << std::setw(6) << std::setfill('0') << frameCounter << ".png"; 

            cv::imwrite(oss.str(), output_img);
            m_logger->log(oss.str() + " saved.");
        }

    }
}    


// Method for stopping camera acquisition
void Driver::Stop()
{
    m_running = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    VmbErrorType err = m_camera->StopContinuousImageAcquisition();
    if (!m_timing) {
    	m_logger->log("Stopped image acquisition.");
    }
    if (err != VmbErrorSuccess)
    {
		m_logger->error("Could not stop acquisition, err=" + std::to_string(err));
        throw std::runtime_error("Could not stop acquisition, err=" + std::to_string(err));
    }
}

// Method to configure a fixed frame rate
void Driver::ConfigureFixedFrameRate()
{
	FeaturePtr pTriggerMode;
	FeaturePtr pAcqMode;
	FeaturePtr pFrameRateEnable;
	FeaturePtr pFrameRate;


	// If --framerate flag is between 0 and 30, set the camera feature accordingly.
	if (m_camera->GetFeatureByName("TriggerMode", pTriggerMode) == VmbErrorSuccess)
	{
		pTriggerMode->SetValue("Off");
	}

	if (m_camera->GetFeatureByName("AcquisitionMode", pAcqMode) == VmbErrorSuccess)
	{
		pAcqMode->SetValue("Continuous");
	}

	if (m_camera->GetFeatureByName("AcquisitionFrameRateEnable", pFrameRateEnable) == VmbErrorSuccess)
	{
		pFrameRateEnable->SetValue(true);
	}
	if ((m_frameRate > 0) && (m_frameRate <= 30)) {
		if (m_camera->GetFeatureByName("AcquisitionFrameRate", pFrameRate) == VmbErrorSuccess)
		{
			pFrameRate->SetValue((double)m_frameRate);
			if (!m_timing) {
				m_logger->log("Frame rate set to " + std::to_string(m_frameRate) + " FPS.");
			}
		}
	}

	else 
	{
		m_logger->error("Frame rate not within allowable boundaries (0 to 30 FPS).");
		throw std::runtime_error("Frame rate not within allowable boundaries (0 to 30 FPS).");
	}
}

// Method that enables a triggered mode
void Driver::ConfigureTriggerMode()
{
	FeaturePtr feature;

	if (m_camera->GetFeatureByName("AcquisitionFrameRateEnable", feature) == VmbErrorSuccess)
	{
		feature->SetValue(false);
	}

	// Enable triggering for frame start
	if (m_camera->GetFeatureByName("TriggerSelector", feature) == VmbErrorSuccess)
	{
		feature->SetValue("FrameStart");
	}

	if (m_camera->GetFeatureByName("TriggerMode", feature) == VmbErrorSuccess)
	{
		feature->SetValue("On");
	}

	// Trigger the camera from software
	if (m_camera->GetFeatureByName("TriggerSource", feature) == VmbErrorSuccess)
	{
		feature->SetValue("Software");
	}

	if (!m_timing) {
		m_logger->log("Camera configured for software trigger.") ;
	}

}

// Method to set exposure time of the camera
void Driver::ConfigureExposureMode()
{
	double minVal, maxVal;

	FeaturePtr pFrameRateEnable;
	FeaturePtr pExposureMode;
	FeaturePtr pExposureTime;
	FeaturePtr pExposureAuto;
	FeaturePtr pGainAuto;
	FeaturePtr pGammaEnable;
	FeaturePtr pGain;
	FeaturePtr pFeature;
	FeaturePtr pTriggerMode;

	double increment;
	double expTimeReadback;
	double finalExposure = m_exposureTime;

	if (m_camera->GetFeatureByName("TriggerMode", pTriggerMode) == VmbErrorSuccess) {
		pTriggerMode->SetValue("Off");
	}

	if (m_camera->GetFeatureByName("AcquisitionFrameRateEnable", pFrameRateEnable) == VmbErrorSuccess) {
		pFrameRateEnable->SetValue(false);
	}
	
	if (m_camera->GetFeatureByName("ExposureMode", pExposureMode) == VmbErrorSuccess) {
		pExposureMode->SetValue("Timed");
	}

	// Turn off automatic exposure so that we can control it manually.
	if (m_camera->GetFeatureByName("ExposureAuto", pExposureAuto) == VmbErrorSuccess) {
		pExposureAuto->SetValue("Off");
	}

	if (m_camera->GetFeatureByName("GainAuto", pGainAuto) == VmbErrorSuccess) {
			pGainAuto->SetValue("Off");
	}

	if (m_camera->GetFeatureByName("GammaEnable", pGammaEnable) == VmbErrorSuccess) {
			pGammaEnable->SetValue(false);
	}

	if (m_camera->GetFeatureByName("Gain", pGain) == VmbErrorSuccess) {
			pGain->SetValue(0.0);
	}

	if (m_camera->GetFeatureByName("ExposureTime", pExposureTime) == VmbErrorSuccess) {
			pExposureTime->GetRange(minVal, maxVal);
			m_logger->debug("Exposure time limits between " + std::to_string(minVal) + " and " + std::to_string(maxVal) + " us.");
	}

	if (pExposureTime->GetIncrement(increment) == VmbErrorSuccess) {
			m_logger->debug("Exposure time increment is " + std::to_string(increment) + " us.");
	}

	if (increment > 0.0) {
		double steps = std::round((m_exposureTime - minVal) / increment); 
		finalExposure = minVal + steps * increment;
	}


	// If exposure time is within limits, set the exposure time appropriately.
	if (m_exposureTime > minVal && m_exposureTime < maxVal) {
		if (m_camera->GetFeatureByName("ExposureTime", pExposureTime) == VmbErrorSuccess) 
		{
			pExposureTime->SetValue(finalExposure);
			if (m_camera->GetFeatureByName("ExposureTime", pFeature) == VmbErrorSuccess)
			{
				pFeature->GetValue(expTimeReadback);
				if (!m_timing) {
					m_logger->log("Camera set to exposure time of " + std::to_string(expTimeReadback) + " us.");
				}
			}
		}
		else
		{
			m_logger->error("Failed to set exposure time to " + std::to_string(m_exposureTime) + " us.");
		}
	}
	else {
		m_logger->error("Exposure time must be set between " + std::to_string(minVal) + " us and " + std::to_string(maxVal) + " us.");
	}

}

// Method to set core locking for the camera.
void Driver::SetCpuAffinity()
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(m_coreid, &cpuset);

	pid_t pid = ::getpid();
	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset) != 0) {
		m_logger->error("Core locking failed.");
		throw std::runtime_error("Core locking failed.");
	} 
	else 
	{
		if (!m_timing) 
		{
			m_logger->log("Pinned to CPU core " + std::to_string(m_coreid));
		}
	}
}

void Driver::SetROI() {
    FeaturePtr pWidth;
    FeaturePtr pHeight;
    FeaturePtr pOffsetX;
    FeaturePtr pOffsetY;

    if (m_camera->GetFeatureByName("Width", pWidth) == VmbErrorSuccess)
    {
        pWidth->SetValue(m_roi.width);
    }
    if (m_camera->GetFeatureByName("Height", pHeight) == VmbErrorSuccess)
    {
        pHeight->SetValue(m_roi.height);
    }
    if (m_camera->GetFeatureByName("OffsetX", pOffsetX) == VmbErrorSuccess)
    {
        pOffsetX->SetValue(m_roi.offsetX);
    }
    if (m_camera->GetFeatureByName("OffsetY", pOffsetY) == VmbErrorSuccess)
    {
        pOffsetY->SetValue(m_roi.offsetY);
    }
    if (!m_timing)
    {
        m_logger->log("ROI Dimensions - W: " + std::to_string(m_roi.width) + " H: " + std::to_string(m_roi.height) + " Offset X: " + std::to_string(m_roi.offsetX) + " Offset Y: " + std::to_string(m_roi.offsetY));
    }
}

} // namespace Examples
} // namespace VmbCPP
