#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>
#include <vector>
#include <memory>

class Logger {
	public:

		// Class constructor, sets up logger class.
		Logger(const std::string& filename, bool enableDebug, std::size_t bufferSize);

		// Class destructor when logger goes out of scope.
		~Logger();

		// Default logging class, minimal verbosity, records timestamps of saved images and format settings at the beginning.
		void log(const std::string& msg);

		// Default logging class, prints runtime errors and exceptions to log file with ERROR code.
		void error(const std::string& msg);

		// Optional logging class, more verbose than general log, records timestamps for triggered acquisitions.
		void debug(const std::string& msg);

		// Save accumulated buffers and flush it to the associated text file.
		void save();

	private:
		std::ofstream logfile;
		bool debugEnabled; 
		std::size_t maxBufferSize;

		std::vector<std::string> buffer;

		std::string timestamp();
		void addToBuffer(const std::string& message);
};

#endif
