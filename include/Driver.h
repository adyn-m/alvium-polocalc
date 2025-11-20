/*=============================================================================
  Copyright (C) 2012-2023 Allied Vision Technologies.  All Rights Reserved.
  Subject to the BSD 3-Clause License.
=============================================================================*/

#ifndef DRIVER_H
#define DRIVER_H

#include "Logger.h"
#include <VmbCPP/VmbCPP.h>
#include <memory>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

namespace VmbCPP {
namespace Examples {


struct ROI {
    VmbInt64_t width = 4128;
    VmbInt64_t height = 3008;
    VmbInt64_t offsetX = 0;
    VmbInt64_t offsetY = 0;
};

class FrameQueue
{
    private:
        std::queue<FramePtr> q;
        std::mutex mtx;
        std::condition_variable cv;
    public:
        void push(const FramePtr& f);

        FramePtr pop();
}; 

		

class Driver
{
private:
    VmbSystem&  m_vmbSystem;
    CameraPtr   m_camera;

    std::string cameraId;
    std::string m_saveDir;
    int		m_frameRate;
    std::string m_mode;
    double		m_exposureTime;
    bool	m_processing;
    std::shared_ptr<::Logger> m_logger;
    bool 	m_timing;
    int		m_coreid;
    ROI     m_roi;
    std::shared_ptr<FrameQueue> m_queue;
    std::thread m_workerThread;
    std::atomic<bool> m_running;

	// Configure trigger settings if --mode "trigger" is selected.
    void ConfigureTriggerMode();

	// Configure fixed frame rate settings if --mode "fixed" is selected.
    void ConfigureFixedFrameRate();

	// Set custom and fixed exposure time if --mode "exposure" is selected. Set exposure time with --exposure, otherwise default 100000 us is used.
    void ConfigureExposureMode();

	// Configure CPU for core locking based on input argument --core.
    void SetCpuAffinity();

    void SetROI();

public:
    /**
     * \brief The constructor will initialize the API and open the given camera
     *
     * \param[in] pCameraId  zero terminated C string with the camera id for the camera to be used
     */
    Driver(const char* cameraId, const std::string& saveDirectory, std::shared_ptr<::Logger> logger, int frameRate, const std::string& mode, int exposureTime, bool processing, bool timing, int core_id, const ROI& roi);

    /**
     * \brief The destructor will stop the acquisition and shutdown the API
     */
    ~Driver();

    /**
     * \brief Start the acquisition.
     */
    void Start();

	// Start the triggered acquisition loop.
	void TriggerFrame();

    // Worker that saves frame separately from acquisition.
    void FrameWorkerLoop();

    /**
     * \brief Stop the acquisition.
     */
    void Stop();

};

}} // namespace VmbCPP

#endif
