#include "Blurring.h"

Blurring::Blurring(RgbQuad** initialRgbInfo, RgbQuad** blurredRgbInfo, const BitmapInfoHeader& fileInfo, unsigned startingIndex, unsigned height)
	: _initialRgbInfo(initialRgbInfo)
	, _blurredRgbInfo(blurredRgbInfo)
	, _fileInfo(fileInfo)
	, _startingIndex(startingIndex)
	, _height(height)
{
}

void Blurring::Execute()
{
    for (unsigned i = _startingIndex; i < _startingIndex + _height; ++i)
    {
        for (unsigned j = 0; j < _fileInfo.biWidth; ++j)
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
                    if (curI >= 0 && curI < _fileInfo.biHeight && curJ >= 0 && curJ < _fileInfo.biWidth)
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
        }
    }
}