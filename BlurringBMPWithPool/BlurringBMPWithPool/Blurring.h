#pragma once

#include "ITask.h"
#include "CommonTypes.h"

class Blurring : public ITask
{
public:
	Blurring(RgbQuad** initialRgbInfo, RgbQuad** blurredRgbInfo, const BitmapInfoHeader& fileInfo, unsigned startingIndex, unsigned height);

	void Execute() override;

private:
	static const int BLURRING_RADIUS = 20;

	RgbQuad** _initialRgbInfo;
	RgbQuad** _blurredRgbInfo;
	BitmapInfoHeader _fileInfo;
	unsigned _startingIndex;
	unsigned _height;
};