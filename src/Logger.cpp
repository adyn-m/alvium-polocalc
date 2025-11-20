#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <memory>



Logger::Logger(const std::string& filename, bool enableDebug, std::size_t bufferSize)
	: debugEnabled(enableDebug), maxBufferSize(bufferSize)
{
	logfile.open(filename, std::ios::out | std::ios::app);
	if (!logfile.is_open()) {
		throw std::runtime_error("Failed to open log file: " + filename);
	}
}

Logger::~Logger() {
	save();
	if (logfile.is_open()) {
		logfile.close();
	}
}

void Logger::log(const std::string& msg) {
	std::string message = "[" + timestamp() + " - INFO] " + msg;
	addToBuffer(message);
}

void Logger::error(const std::string& msg) {
	std::string message = "[" + timestamp() + " - ERROR] " + msg;
	addToBuffer(message);
}

void Logger::debug(const std::string& msg) {
    if (debugEnabled) {
	    std::string message = "[" + timestamp() + " - DEBUG] " + msg;
	    addToBuffer(message);
    }
}

void Logger::addToBuffer(const std::string& message) {
	buffer.push_back(message);

	if (buffer.size() >= maxBufferSize) {
		save();
	}
}

void Logger::save() {
	if (!logfile.is_open()) return;

	for (const auto& entry : buffer) {
		logfile << entry << std::endl;
	}
	logfile.flush();
	buffer.clear();
}


std::string Logger::timestamp() {
	using namespace std::chrono;
	auto now = system_clock::now();
	auto t = system_clock::to_time_t(now);
	auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream oss;
	oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
	    << "." << std::setw(3) <<std::setfill('0') << ms.count();

	return oss.str();
}
