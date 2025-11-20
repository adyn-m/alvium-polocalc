#include "Logger.h"

#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string>
#include <VmbCPP/VmbCPP.h>
#include <atomic>
#include <csignal>
#include <thread>
#include <condition_variable>
#include <vector>
#include <sstream>
#include <iostream>

static struct termios old, current;

namespace {

		volatile std::sig_atomic_t c_stop = 0;

		void signal_handler(int signal)
		{
				if (signal == SIGINT) 
				{
						c_stop = 1;
				}
		}
}

void SetupSignalHandler()
{
		std::signal(SIGINT, signal_handler);
}

bool StopRequested()
{
		return c_stop != 0 ;
}

void initTermios()
{
		tcgetattr(STDIN_FILENO, &old);
		current = old;
		current.c_lflag &= ~(ICANON | ECHO);
		current.c_cc[VMIN] = 0;
		current.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &current);

		fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void resetTermios(void)
{
		tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

char getch() {
		char ch = 0;
		if (read(STDIN_FILENO, &ch, 1) > 0) {
			return ch;
		}
		return 0;
}

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delimiter)) tokens.push_back(item);
    return tokens;
}



namespace VmbCPP {
std::string PixelFormatToString(VmbPixelFormatType pf)
{
		switch(pf)
		{
				case VmbPixelFormatMono8:		return "Mono8";
				case VmbPixelFormatMono10:		return "Mono10";
				case VmbPixelFormatMono12:		return "Mono12";
				case VmbPixelFormatBayerRG8:	return "BayerRG8";
				case VmbPixelFormatBayerBG8:	return "BayerBG8";
				case VmbPixelFormatBayerGR8:	return "BayerGR8";
				case VmbPixelFormatBayerGB8:	return "BayerGB8";
				case VmbPixelFormatRgb8:		return "RGB8";
				case VmbPixelFormatRgb16:		return "RGB16";
				default: return "Unknown";
		}
}
}


