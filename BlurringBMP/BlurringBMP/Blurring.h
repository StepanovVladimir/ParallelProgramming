#pragma once

#include "CommonTypes.h"

RgbQuad** blurImage(RgbQuad** rgbInfo, const BitmapInfoHeader& fileInfoHeader, unsigned threadsCount, unsigned processorsCount, int* threadsPriorities, unsigned startTime);