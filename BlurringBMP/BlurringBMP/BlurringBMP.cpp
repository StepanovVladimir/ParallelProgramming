#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <timeapi.h>

using namespace std;

struct Ciexyz
{
    int cieX;
    int cieY;
    int cieZ;
};

struct CiexyzTriple
{
    Ciexyz ciexyzRed;
    Ciexyz ciexyzGreen;
    Ciexyz ciexyzBlue;
};

struct BitmapFileHeader
{
    unsigned short bfType;
    unsigned int bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;
};

struct BitmapInfoHeader
{
    unsigned int biSize;
    unsigned int biWidth;
    unsigned int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    unsigned int biXPelsPerMeter;
    unsigned int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
    unsigned int biRedMask;
    unsigned int biGreenMask;
    unsigned int biBlueMask;
    unsigned int biAlphaMask;
    unsigned int biCSType;
    CiexyzTriple biEndpoints;
    unsigned int biGammaRed;
    unsigned int biGammaGreen;
    unsigned int biGammaBlue;
    unsigned int biIntent;
    unsigned int biProfileData;
    unsigned int biProfileSize;
    unsigned int biReserved;
};

struct RgbQuad
{
    unsigned char rgbBlue;
    unsigned char rgbGreen;
    unsigned char rgbRed;
    unsigned char rgbReserved;
};

struct ThreadData
{
    unsigned width;
    unsigned height;
    unsigned startingIndex;
    unsigned threadHeight;
    RgbQuad** rgbInfo;
};

template <typename Type>
void read(istream& strm, Type& result, size_t size)
{
    strm.read(reinterpret_cast<char*>(&result), size);
}

template <typename Type>
void write(ostream& strm, const Type& source, size_t size)
{
    strm.write(reinterpret_cast<const char*>(&source), size);
}

unsigned char bitextract(const unsigned int byte, const unsigned int mask)
{
    if (mask == 0)
    {
        return 0;
    }

    // определение количества нулевых бит справа от маски
    int maskBufer = mask;
    int maskPadding = 0;

    while (!(maskBufer & 1))
    {
        maskBufer >>= 1;
        ++maskPadding;
    }

    // применение маски и смещение
    return (byte & mask) >> maskPadding;
}

RgbQuad** readBmpFile(const string& fileName, BitmapFileHeader& fileHeader, BitmapInfoHeader& fileInfoHeader)
{
    // открываем файл
    ifstream inFile(fileName, ifstream::binary);
    if (!inFile)
    {
        throw runtime_error("Error opening file '" + fileName + "'.");
    }

    // заголовк изображения
    read(inFile, fileHeader.bfType, sizeof(fileHeader.bfType));
    read(inFile, fileHeader.bfSize, sizeof(fileHeader.bfSize));
    read(inFile, fileHeader.bfReserved1, sizeof(fileHeader.bfReserved1));
    read(inFile, fileHeader.bfReserved2, sizeof(fileHeader.bfReserved2));
    read(inFile, fileHeader.bfOffBits, sizeof(fileHeader.bfOffBits));

    if (fileHeader.bfType != 0x4D42)
    {
        throw runtime_error("Error: '" + fileName + "' is not BMP file.");
    }

    // информация изображения
    read(inFile, fileInfoHeader.biSize, sizeof(fileInfoHeader.biSize));

    // bmp core
    if (fileInfoHeader.biSize >= 12)
    {
        read(inFile, fileInfoHeader.biWidth, sizeof(fileInfoHeader.biWidth));
        read(inFile, fileInfoHeader.biHeight, sizeof(fileInfoHeader.biHeight));
        read(inFile, fileInfoHeader.biPlanes, sizeof(fileInfoHeader.biPlanes));
        read(inFile, fileInfoHeader.biBitCount, sizeof(fileInfoHeader.biBitCount));
    }

    // получаем информацию о битности
    int colorsCount = fileInfoHeader.biBitCount >> 3;
    if (colorsCount < 3)
    {
        colorsCount = 3;
    }

    int bitsOnColor = fileInfoHeader.biBitCount / colorsCount;
    int maskValue = (1 << bitsOnColor) - 1;

    // bmp v1
    if (fileInfoHeader.biSize >= 40)
    {
        read(inFile, fileInfoHeader.biCompression, sizeof(fileInfoHeader.biCompression));
        read(inFile, fileInfoHeader.biSizeImage, sizeof(fileInfoHeader.biSizeImage));
        read(inFile, fileInfoHeader.biXPelsPerMeter, sizeof(fileInfoHeader.biXPelsPerMeter));
        read(inFile, fileInfoHeader.biYPelsPerMeter, sizeof(fileInfoHeader.biYPelsPerMeter));
        read(inFile, fileInfoHeader.biClrUsed, sizeof(fileInfoHeader.biClrUsed));
        read(inFile, fileInfoHeader.biClrImportant, sizeof(fileInfoHeader.biClrImportant));
    }

    // bmp v2
    fileInfoHeader.biRedMask = 0;
    fileInfoHeader.biGreenMask = 0;
    fileInfoHeader.biBlueMask = 0;

    if (fileInfoHeader.biSize >= 52)
    {
        read(inFile, fileInfoHeader.biRedMask, sizeof(fileInfoHeader.biRedMask));
        read(inFile, fileInfoHeader.biGreenMask, sizeof(fileInfoHeader.biGreenMask));
        read(inFile, fileInfoHeader.biBlueMask, sizeof(fileInfoHeader.biBlueMask));
    }

    // если маска не задана, то ставим маску по умолчанию
    if (fileInfoHeader.biRedMask == 0 || fileInfoHeader.biGreenMask == 0 || fileInfoHeader.biBlueMask == 0)
    {
        fileInfoHeader.biRedMask = maskValue << (bitsOnColor * 2);
        fileInfoHeader.biGreenMask = maskValue << bitsOnColor;
        fileInfoHeader.biBlueMask = maskValue;
    }

    // bmp v3
    if (fileInfoHeader.biSize >= 56)
    {
        read(inFile, fileInfoHeader.biAlphaMask, sizeof(fileInfoHeader.biAlphaMask));
    }
    else
    {
        fileInfoHeader.biAlphaMask = maskValue << (bitsOnColor * 3);
    }

    // bmp v4
    if (fileInfoHeader.biSize >= 108)
    {
        read(inFile, fileInfoHeader.biCSType, sizeof(fileInfoHeader.biCSType));
        read(inFile, fileInfoHeader.biEndpoints, sizeof(fileInfoHeader.biEndpoints));
        read(inFile, fileInfoHeader.biGammaRed, sizeof(fileInfoHeader.biGammaRed));
        read(inFile, fileInfoHeader.biGammaGreen, sizeof(fileInfoHeader.biGammaGreen));
        read(inFile, fileInfoHeader.biGammaBlue, sizeof(fileInfoHeader.biGammaBlue));
    }

    // bmp v5
    if (fileInfoHeader.biSize >= 124)
    {
        read(inFile, fileInfoHeader.biIntent, sizeof(fileInfoHeader.biIntent));
        read(inFile, fileInfoHeader.biProfileData, sizeof(fileInfoHeader.biProfileData));
        read(inFile, fileInfoHeader.biProfileSize, sizeof(fileInfoHeader.biProfileSize));
        read(inFile, fileInfoHeader.biReserved, sizeof(fileInfoHeader.biReserved));
    }

    // проверка на поддерку этой версии формата
    if (fileInfoHeader.biSize != 12 && fileInfoHeader.biSize != 40 && fileInfoHeader.biSize != 52 &&
        fileInfoHeader.biSize != 56 && fileInfoHeader.biSize != 108 && fileInfoHeader.biSize != 124)
    {
        cout << "Error: Unsupported BMP format." << endl;
        return 0;
    }

    if (fileInfoHeader.biBitCount != 16 && fileInfoHeader.biBitCount != 24 && fileInfoHeader.biBitCount != 32)
    {
        cout << "Error: Unsupported BMP bit count." << endl;
        return 0;
    }

    if (fileInfoHeader.biCompression != 0 && fileInfoHeader.biCompression != 3)
    {
        cout << "Error: Unsupported BMP compression." << endl;
        return 0;
    }

    // rgb info
    RgbQuad** rgbInfo = new RgbQuad*[fileInfoHeader.biHeight];

    for (unsigned i = 0; i < fileInfoHeader.biHeight; i++)
    {
        rgbInfo[i] = new RgbQuad[fileInfoHeader.biWidth];
    }

    // определение размера отступа в конце каждой строки
    int linePadding = ((fileInfoHeader.biWidth * (fileInfoHeader.biBitCount / 8)) % 4) & 3;

    // чтение
    unsigned bufer;

    for (unsigned i = 0; i < fileInfoHeader.biHeight; ++i)
    {
        for (unsigned j = 0; j < fileInfoHeader.biWidth; ++j)
        {
            read(inFile, bufer, fileInfoHeader.biBitCount / 8);

            rgbInfo[i][j].rgbRed = bitextract(bufer, fileInfoHeader.biRedMask);
            rgbInfo[i][j].rgbGreen = bitextract(bufer, fileInfoHeader.biGreenMask);
            rgbInfo[i][j].rgbBlue = bitextract(bufer, fileInfoHeader.biBlueMask);
            rgbInfo[i][j].rgbReserved = bitextract(bufer, fileInfoHeader.biAlphaMask);
        }
        inFile.seekg(linePadding, ios_base::cur);
    }

    return rgbInfo;
}

unsigned stringToThreadsCount(const string& str)
{
    int threadsCount = stoi(str);
    if (threadsCount < 1 || threadsCount > 16)
    {
        throw runtime_error("Threads count can be from 1 to 16");
    }
    return (unsigned)threadsCount;
}

unsigned stringToProcessorsCount(const string& str)
{
    int processorsCount = stoi(str);
    if (processorsCount < 1 || processorsCount > 4)
    {
        throw runtime_error("Processors count can be from 1 to 4");
    }
    return (unsigned)processorsCount;
}

DWORD WINAPI threadProc(CONST LPVOID lpParam)
{
    ThreadData* threadData = (ThreadData*)lpParam;
    
    for (unsigned i = threadData->startingIndex; i < threadData->startingIndex + threadData->threadHeight; ++i)
    {
        for (unsigned j = 0; j < threadData->width; ++j)
        {
            unsigned sumR = 0;
            unsigned sumG = 0;
            unsigned sumB = 0;
            unsigned pixelsCount = 0;
            for (int di = -10; di <= 10; ++di)
            {
                for (int dj = -10; dj <= 10; ++dj)
                {
                    unsigned curI = i + di;
                    unsigned curJ = j + dj;
                    if (curI >= 0 && curI < threadData->height && curJ >= 0 && curJ < threadData->width)
                    {
                        sumR += threadData->rgbInfo[curI][curJ].rgbRed;
                        sumG += threadData->rgbInfo[curI][curJ].rgbGreen;
                        sumB += threadData->rgbInfo[curI][curJ].rgbBlue;
                        ++pixelsCount;
                    }
                }
            }

            threadData->rgbInfo[i][j].rgbRed = sumR / pixelsCount;
            threadData->rgbInfo[i][j].rgbGreen = sumG / pixelsCount;
            threadData->rgbInfo[i][j].rgbBlue = sumB / pixelsCount;
        }
    }

    ExitThread(0);
}

void blurImage(RgbQuad** rgbInfo, const BitmapInfoHeader& fileInfoHeader, unsigned threadsCount, unsigned processorsCount)
{
    HANDLE* handles = new HANDLE[threadsCount];
    ThreadData* threadsData = new ThreadData[threadsCount];

    unsigned threadHeight = fileInfoHeader.biHeight / threadsCount;
    unsigned startingIndex = 0;
    unsigned affinityMask = (1 << processorsCount) - 1;
    for (unsigned i = 0; i < threadsCount - 1; ++i)
    {
        threadsData[i].width = fileInfoHeader.biWidth;
        threadsData[i].height = fileInfoHeader.biHeight;
        threadsData[i].startingIndex = startingIndex;
        threadsData[i].threadHeight = threadHeight;
        threadsData[i].rgbInfo = rgbInfo;

        handles[i] = CreateThread(NULL, 0, &threadProc, &threadsData[i], CREATE_SUSPENDED, NULL);
        SetThreadAffinityMask(handles[i], affinityMask);
        ResumeThread(handles[i]);

        startingIndex += threadHeight;
    }

    threadsData[threadsCount - 1].width = fileInfoHeader.biWidth;
    threadsData[threadsCount - 1].height = fileInfoHeader.biHeight;
    threadsData[threadsCount - 1].startingIndex = startingIndex;
    threadsData[threadsCount - 1].threadHeight = fileInfoHeader.biHeight - startingIndex;
    threadsData[threadsCount - 1].rgbInfo = rgbInfo;

    handles[threadsCount - 1] = CreateThread(NULL, 0, &threadProc, &threadsData[threadsCount - 1], CREATE_SUSPENDED, NULL);
    SetThreadAffinityMask(handles[threadsCount - 1], affinityMask);
    ResumeThread(handles[threadsCount - 1]);

    WaitForMultipleObjects(threadsCount, handles, true, INFINITE);
}

void writeBmpFile(const string& fileName, RgbQuad** rgbInfo, const BitmapFileHeader& fileHeader, const BitmapInfoHeader& fileInfoHeader)
{
    ofstream outFile(fileName, ifstream::binary);

    write(outFile, fileHeader.bfType, sizeof(fileHeader.bfType));
    write(outFile, fileHeader.bfSize, sizeof(fileHeader.bfSize));
    write(outFile, fileHeader.bfReserved1, sizeof(fileHeader.bfReserved1));
    write(outFile, fileHeader.bfReserved2, sizeof(fileHeader.bfReserved2));
    write(outFile, fileHeader.bfOffBits, sizeof(fileHeader.bfOffBits));

    write(outFile, fileInfoHeader.biSize, sizeof(fileInfoHeader.biSize));

    write(outFile, fileInfoHeader.biWidth, sizeof(fileInfoHeader.biWidth));
    write(outFile, fileInfoHeader.biHeight, sizeof(fileInfoHeader.biHeight));
    write(outFile, fileInfoHeader.biPlanes, sizeof(fileInfoHeader.biPlanes));
    write(outFile, fileInfoHeader.biBitCount, sizeof(fileInfoHeader.biBitCount));

    write(outFile, fileInfoHeader.biCompression, sizeof(fileInfoHeader.biCompression));
    write(outFile, fileInfoHeader.biSizeImage, sizeof(fileInfoHeader.biSizeImage));
    write(outFile, fileInfoHeader.biXPelsPerMeter, sizeof(fileInfoHeader.biXPelsPerMeter));
    write(outFile, fileInfoHeader.biYPelsPerMeter, sizeof(fileInfoHeader.biYPelsPerMeter));
    write(outFile, fileInfoHeader.biClrUsed, sizeof(fileInfoHeader.biClrUsed));
    write(outFile, fileInfoHeader.biClrImportant, sizeof(fileInfoHeader.biClrImportant));

    write(outFile, fileInfoHeader.biRedMask, sizeof(fileInfoHeader.biRedMask));
    write(outFile, fileInfoHeader.biGreenMask, sizeof(fileInfoHeader.biGreenMask));
    write(outFile, fileInfoHeader.biBlueMask, sizeof(fileInfoHeader.biBlueMask));

    write(outFile, fileInfoHeader.biAlphaMask, sizeof(fileInfoHeader.biAlphaMask));

    write(outFile, fileInfoHeader.biCSType, sizeof(fileInfoHeader.biCSType));
    write(outFile, fileInfoHeader.biEndpoints, sizeof(fileInfoHeader.biEndpoints));
    write(outFile, fileInfoHeader.biGammaRed, sizeof(fileInfoHeader.biGammaRed));
    write(outFile, fileInfoHeader.biGammaGreen, sizeof(fileInfoHeader.biGammaGreen));
    write(outFile, fileInfoHeader.biGammaBlue, sizeof(fileInfoHeader.biGammaBlue));

    write(outFile, fileInfoHeader.biIntent, sizeof(fileInfoHeader.biIntent));
    write(outFile, fileInfoHeader.biProfileData, sizeof(fileInfoHeader.biProfileData));
    write(outFile, fileInfoHeader.biProfileSize, sizeof(fileInfoHeader.biProfileSize));
    write(outFile, fileInfoHeader.biReserved, sizeof(fileInfoHeader.biReserved));

    for (unsigned i = 0; i < fileInfoHeader.biHeight; ++i)
    {
        for (unsigned j = 0; j < fileInfoHeader.biWidth; ++j)
        {
            write(outFile, rgbInfo[i][j].rgbBlue, sizeof(rgbInfo[i][j].rgbBlue));
            write(outFile, rgbInfo[i][j].rgbGreen, sizeof(rgbInfo[i][j].rgbGreen));
            write(outFile, rgbInfo[i][j].rgbRed, sizeof(rgbInfo[i][j].rgbRed));
            write(outFile, rgbInfo[i][j].rgbReserved, sizeof(rgbInfo[i][j].rgbReserved));
        }
    }
}

int main(int argc, char* argv[])
{
    unsigned startTime = timeGetTime();

    if (argc < 5)
    {
        cout << "Usage: " << argv[0] << " input_file_name output_file_name threads_count processors_count" << endl;
        return 1;
    }

    BitmapFileHeader fileHeader;
    BitmapInfoHeader fileInfoHeader;
    RgbQuad** rgbInfo;
    unsigned threadsCount;
    unsigned processorsCount;
    try
    {
        rgbInfo = readBmpFile(argv[1], fileHeader, fileInfoHeader);
        threadsCount = stringToThreadsCount(argv[3]);
        processorsCount = stringToProcessorsCount(argv[4]);
    }
    catch (const runtime_error& e)
    {
        cout << e.what() << endl;
        return 1;
    }
    catch (const invalid_argument& e)
    {
        cout << e.what() << endl;
        return 1;
    }
    catch (const out_of_range& e)
    {
        cout << e.what() << endl;
        return 1;
    }

    blurImage(rgbInfo, fileInfoHeader, threadsCount, processorsCount);
    writeBmpFile(argv[2], rgbInfo, fileHeader, fileInfoHeader);

    cout << timeGetTime() - startTime;

    return 0;
}