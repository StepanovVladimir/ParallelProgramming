#pragma once

#include "LogBuffer.h"
#include <fstream>
#include <string>

template<typename T>
class LogFileWriter
{
public:
	LogFileWriter()
		: _outFile("log.txt")
	{
	}

	void write(const LogBuffer<T>& logBuffer)
	{
		for (T data : logBuffer)
		{
			_outFile << data << std::endl;
		}
	}

private:
	std::ofstream _outFile;
};