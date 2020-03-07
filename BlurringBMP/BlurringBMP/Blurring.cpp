#include "Blurring.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <timeapi.h>

using namespace std;

const uint8_t BLURRING_RADIUS = 20;

struct ThreadData
{
    unsigned startingIndex;
    unsigned height;
    unsigned threadNumber;
};

unsigned _width;
unsigned _height;
RgbQuad** _initialRgbInfo;
RgbQuad** _blurredRgbInfo;
unsigned _startTime;

DWORD WINAPI threadProc(CONST LPVOID lpParam)
{
    ThreadData* threadData = (ThreadData*)lpParam;
    ofstream outFile(to_string(threadData->threadNumber) + ".txt");
    unsigned time = 0;
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

            unsigned curTime = timeGetTime() - _startTime;
            if (curTime > time)
            {
                outFile << curTime << endl;
                time = curTime;
            }
            
        }
    }

    ExitThread(0);
}

RgbQuad** blurImage(RgbQuad** rgbInfo, const BitmapInfoHeader& fileInfoHeader, unsigned threadsCount, unsigned processorsCount, int* threadsPriorities, unsigned startTime)
{
    _width = fileInfoHeader.biWidth;
    _height = fileInfoHeader.biHeight;
    _initialRgbInfo = rgbInfo;
    _blurredRgbInfo = new RgbQuad*[_height];
    for (unsigned i = 0; i < _height; i++)
    {
        _blurredRgbInfo[i] = new RgbQuad[_width];
    }
    _startTime = startTime;

    HANDLE* handles = new HANDLE[threadsCount];
    ThreadData* threadsData = new ThreadData[threadsCount];

    unsigned threadHeight = fileInfoHeader.biHeight / threadsCount;
    unsigned startingIndex = 0;
    unsigned affinityMask = (1 << processorsCount) - 1;
    for (unsigned i = 0; i < threadsCount - 1; ++i)
    {
        threadsData[i].startingIndex = startingIndex;
        threadsData[i].height = threadHeight;
        threadsData[i].threadNumber = i + 1;

        handles[i] = CreateThread(NULL, 0, &threadProc, &threadsData[i], CREATE_SUSPENDED, NULL);
        SetThreadAffinityMask(handles[i], affinityMask);
        SetThreadPriority(handles[i], threadsPriorities[i]);
        ResumeThread(handles[i]);

        startingIndex += threadHeight;
    }

    threadsData[threadsCount - 1].startingIndex = startingIndex;
    threadsData[threadsCount - 1].height = fileInfoHeader.biHeight - startingIndex;
    threadsData[threadsCount - 1].threadNumber = threadsCount;

    handles[threadsCount - 1] = CreateThread(NULL, 0, &threadProc, &threadsData[threadsCount - 1], CREATE_SUSPENDED, NULL);
    SetThreadAffinityMask(handles[threadsCount - 1], affinityMask);
    SetThreadPriority(handles[threadsCount - 1], threadsPriorities[threadsCount - 1]);
    ResumeThread(handles[threadsCount - 1]);

    WaitForMultipleObjects(threadsCount, handles, true, INFINITE);

    return _blurredRgbInfo;
}