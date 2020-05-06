#include "Blurring.h"
#include "LogFileWriter.h"
#include <windows.h>
#include <string>
#include <timeapi.h>

using namespace std;

const uint8_t BLURRING_RADIUS = 20;

struct ThreadData
{
    unsigned startingIndex;
    unsigned height;
};

unsigned _width;
unsigned _height;
RgbQuad** _initialRgbInfo;
RgbQuad** _blurredRgbInfo;
unsigned _startTime;
CRITICAL_SECTION _criticalSection;
LogBuffer<unsigned> _logBuffer;
LogFileWriter<unsigned> _writer;

DWORD WINAPI logSizeMonitoringThread(LPVOID lpParam)
{
    LogBuffer<unsigned>* tmpBuffer = (LogBuffer<unsigned>*)lpParam;
    _writer.write(*tmpBuffer);
    delete tmpBuffer;
    ExitThread(0);
}

DWORD WINAPI threadProc(CONST LPVOID lpParam)
{
    ThreadData* threadData = (ThreadData*)lpParam;
    for (unsigned i = threadData->startingIndex; i < threadData->startingIndex + threadData->height; ++i)
    {
        for (unsigned j = 0; j < _width; ++j)
        {
            unsigned sumR = 0;
            unsigned sumG = 0;
            unsigned sumB = 0;
            unsigned pixelsCount = 0;
            for (int di = -BLURRING_RADIUS; di <= BLURRING_RADIUS; ++di)
            {
                for (int dj = -BLURRING_RADIUS; dj <= BLURRING_RADIUS; ++dj)
                {
                    unsigned curI = i + di;
                    unsigned curJ = j + dj;
                    if (curI >= 0 && curI < _height && curJ >= 0 && curJ < _width)
                    {
                        sumR += _initialRgbInfo[curI][curJ].rgbRed;
                        sumG += _initialRgbInfo[curI][curJ].rgbGreen;
                        sumB += _initialRgbInfo[curI][curJ].rgbBlue;
                        ++pixelsCount;
                    }
                }
            }

            _blurredRgbInfo[i][j].rgbRed = sumR / pixelsCount;
            _blurredRgbInfo[i][j].rgbGreen = sumG / pixelsCount;
            _blurredRgbInfo[i][j].rgbBlue = sumB / pixelsCount;

            _logBuffer.Log(timeGetTime() - _startTime);

            EnterCriticalSection(&_criticalSection);
            if (_logBuffer.GetSize() >= 50000)
            {
                LogBuffer<unsigned>* tmpBuffer = new LogBuffer<unsigned>(move(_logBuffer));
                HANDLE handle = CreateThread(NULL, 0, &logSizeMonitoringThread, tmpBuffer, CREATE_SUSPENDED, NULL);
                ResumeThread(handle);
            }
            LeaveCriticalSection(&_criticalSection);
        }
    }

    ExitThread(0);
}

RgbQuad** blurImage(RgbQuad** rgbInfo, const BitmapInfoHeader& fileInfoHeader, unsigned threadsCount, unsigned processorsCount, int* threadsPriorities, unsigned startTime)
{
    if (!InitializeCriticalSectionAndSpinCount(&_criticalSection, 0x00000400))
    {
        throw runtime_error("Failed to initialize critical section");
    }

    _width = fileInfoHeader.biWidth;
    _height = fileInfoHeader.biHeight;
    _initialRgbInfo = rgbInfo;
    _blurredRgbInfo = new RgbQuad*[_height];
    for (unsigned i = 0; i < _height; i++)
    {
        _blurredRgbInfo[i] = new RgbQuad[_width];
    }
    _startTime = startTime;
    _logBuffer.AddCriticalSection(&_criticalSection);

    HANDLE* handles = new HANDLE[threadsCount];
    ThreadData* threadsData = new ThreadData[threadsCount];

    unsigned threadHeight = fileInfoHeader.biHeight / threadsCount;
    unsigned startingIndex = 0;
    unsigned affinityMask = (1 << processorsCount) - 1;
    for (unsigned i = 0; i < threadsCount - 1; ++i)
    {
        threadsData[i].startingIndex = startingIndex;
        threadsData[i].height = threadHeight;

        handles[i] = CreateThread(NULL, 0, &threadProc, &threadsData[i], CREATE_SUSPENDED, NULL);
        SetThreadAffinityMask(handles[i], affinityMask);
        SetThreadPriority(handles[i], threadsPriorities[i]);
        ResumeThread(handles[i]);

        startingIndex += threadHeight;
    }

    threadsData[threadsCount - 1].startingIndex = startingIndex;
    threadsData[threadsCount - 1].height = fileInfoHeader.biHeight - startingIndex;

    handles[threadsCount - 1] = CreateThread(NULL, 0, &threadProc, &threadsData[threadsCount - 1], CREATE_SUSPENDED, NULL);
    SetThreadAffinityMask(handles[threadsCount - 1], affinityMask);
    SetThreadPriority(handles[threadsCount - 1], threadsPriorities[threadsCount - 1]);
    ResumeThread(handles[threadsCount - 1]);

    WaitForMultipleObjects(threadsCount, handles, true, INFINITE);

    DeleteCriticalSection(&_criticalSection);

    LogBuffer<unsigned>* tmpBuffer = new LogBuffer<unsigned>(move(_logBuffer));
    HANDLE handle = CreateThread(NULL, 0, &logSizeMonitoringThread, tmpBuffer, CREATE_SUSPENDED, NULL);
    ResumeThread(handle);

    return _blurredRgbInfo;
}