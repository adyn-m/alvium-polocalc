#ifndef UTILS_H
#define UTILS_H

#include <termios.h>
#include <stdio.h>
#include <VmbCPP/VmbCPP.h>
#include <vector>


// Initialize CTRL+C signal handler
void SetupSignalHandler(); 

// Ends the loop if CTRL+C pressed
bool StopRequested();

// Initialize new terminal I/O settings
void initTermios();

// Restore old terminal I/O settings
void resetTermios(void);

// Read 1 character
char getch(void);

// Split a command line string argument using a specified delimiter
std::vector<std::string> split(const std::string& s, char delimiter);

// Convert the pixel format to a readable string.
namespace VmbCPP {
std::string PixelFormatToString(VmbPixelFormatType pixelFormat);
}

#endif 
