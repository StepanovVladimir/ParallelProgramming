#include "Blurring.h"
#include "Worker.h"
#include "Pool.h"
#include <iostream>
#include <fstream>
#include <string>
#include <windows.h>
#include <timeapi.h>
#include <vector>

using namespace std;

unsigned stringToThreadsCount(const string& str)
{
    int threadsCount = stoi(str);
    if (threadsCount < 1 || threadsCount > 16)
    {
        throw runtime_error("Threads count can be from 1 to 16");
    }
    return (unsigned)threadsCount;
}

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
        throw runtime_error("Error: Unsupported BMP format.");
    }

    if (fileInfoHeader.biBitCount != 16 && fileInfoHeader.biBitCount != 24 && fileInfoHeader.biBitCount != 32)
    {
        throw runtime_error("Error: Unsupported BMP bit count.");
    }

    if (fileInfoHeader.biCompression != 0 && fileInfoHeader.biCompression != 3)
    {
        throw runtime_error("Error: Unsupported BMP compression.");
    }

    // rgb info
    RgbQuad** rgbInfo = new RgbQuad * [fileInfoHeader.biHeight];

    for (unsigned i = 0; i < fileInfoHeader.biHeight; i++)
    {
        rgbInfo[i] = new RgbQuad[fileInfoHeader.biWidth];
    }

    // определение размера отступа в конце каждой строки
    int linePadding = ((fileInfoHeader.biWidth * (fileInfoHeader.biBitCount / 8)) % 4);

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

    if (fileInfoHeader.biSize >= 40)
    {
        write(outFile, fileInfoHeader.biCompression, sizeof(fileInfoHeader.biCompression));
        write(outFile, fileInfoHeader.biSizeImage, sizeof(fileInfoHeader.biSizeImage));
        write(outFile, fileInfoHeader.biXPelsPerMeter, sizeof(fileInfoHeader.biXPelsPerMeter));
        write(outFile, fileInfoHeader.biYPelsPerMeter, sizeof(fileInfoHeader.biYPelsPerMeter));
        write(outFile, fileInfoHeader.biClrUsed, sizeof(fileInfoHeader.biClrUsed));
        write(outFile, fileInfoHeader.biClrImportant, sizeof(fileInfoHeader.biClrImportant));
    }

    if (fileInfoHeader.biSize >= 52)
    {
        write(outFile, fileInfoHeader.biRedMask, sizeof(fileInfoHeader.biRedMask));
        write(outFile, fileInfoHeader.biGreenMask, sizeof(fileInfoHeader.biGreenMask));
        write(outFile, fileInfoHeader.biBlueMask, sizeof(fileInfoHeader.biBlueMask));
    }

    if (fileInfoHeader.biSize >= 56)
    {
        write(outFile, fileInfoHeader.biAlphaMask, sizeof(fileInfoHeader.biAlphaMask));
    }

    if (fileInfoHeader.biSize >= 108)
    {
        write(outFile, fileInfoHeader.biCSType, sizeof(fileInfoHeader.biCSType));
        write(outFile, fileInfoHeader.biEndpoints, sizeof(fileInfoHeader.biEndpoints));
        write(outFile, fileInfoHeader.biGammaRed, sizeof(fileInfoHeader.biGammaRed));
        write(outFile, fileInfoHeader.biGammaGreen, sizeof(fileInfoHeader.biGammaGreen));
        write(outFile, fileInfoHeader.biGammaBlue, sizeof(fileInfoHeader.biGammaBlue));
    }

    if (fileInfoHeader.biSize >= 124)
    {
        write(outFile, fileInfoHeader.biIntent, sizeof(fileInfoHeader.biIntent));
        write(outFile, fileInfoHeader.biProfileData, sizeof(fileInfoHeader.biProfileData));
        write(outFile, fileInfoHeader.biProfileSize, sizeof(fileInfoHeader.biProfileSize));
        write(outFile, fileInfoHeader.biReserved, sizeof(fileInfoHeader.biReserved));
    }

    for (unsigned i = 0; i < fileInfoHeader.biHeight; ++i)
    {
        for (unsigned j = 0; j < fileInfoHeader.biWidth; ++j)
        {
            write(outFile, rgbInfo[i][j].rgbBlue, sizeof(rgbInfo[i][j].rgbBlue));
            write(outFile, rgbInfo[i][j].rgbGreen, sizeof(rgbInfo[i][j].rgbGreen));
            write(outFile, rgbInfo[i][j].rgbRed, sizeof(rgbInfo[i][j].rgbRed));
            if (fileInfoHeader.biBitCount == 32)
            {
                write(outFile, rgbInfo[i][j].rgbReserved, sizeof(rgbInfo[i][j].rgbReserved));
            }
        }
    }
}

vector<RgbQuad**> blurImages(vector<RgbQuad**> initialRgbInfos, vector<BitmapInfoHeader> fileInfoHeaders, unsigned blocksCount)
{
    vector<Worker> workers;
    vector<ITask*> tasks;
    vector<RgbQuad**> blurredRgbInfos;
    for (int i = 0; i < fileInfoHeaders.size(); ++i)
    {
        RgbQuad** blurredRgbInfo = new RgbQuad*[fileInfoHeaders[i].biHeight];
        for (unsigned j = 0; j < fileInfoHeaders[i].biHeight; j++)
        {
            blurredRgbInfo[j] = new RgbQuad[fileInfoHeaders[i].biWidth];
        }
        blurredRgbInfos.push_back(blurredRgbInfo);

        unsigned height = fileInfoHeaders[i].biHeight / blocksCount;
        unsigned startingIndex = 0;
        for (unsigned j = 0; j < blocksCount - 1; ++j)
        {
            workers.push_back(Worker());
            tasks.push_back(new Blurring(initialRgbInfos[i], blurredRgbInfos[i], fileInfoHeaders[i], startingIndex, height));
            startingIndex += height;
        }

        height = fileInfoHeaders[i].biHeight - startingIndex;
        workers.push_back(Worker());
        tasks.push_back(new Blurring(initialRgbInfos[i], blurredRgbInfos[i], fileInfoHeaders[i], startingIndex, height));
    }

    unsigned startTime = timeGetTime();

    for (int i = 0; i < workers.size(); i++)
    {
        workers[i].ExecuteTask(tasks[i]);
    }

    for (Worker worker : workers)
    {
        worker.Wait();
    }

    cout << timeGetTime() - startTime;

    return blurredRgbInfos;
}

vector<RgbQuad**> blurImagesWithPool(vector<RgbQuad**> initialRgbInfos, vector<BitmapInfoHeader> fileInfoHeaders, unsigned blocksCount, unsigned threadsCount)
{
    vector<ITask*> tasks;
    vector<RgbQuad**> blurredRgbInfos;
    for (int i = 0; i < fileInfoHeaders.size(); ++i)
    {
        RgbQuad** blurredRgbInfo = new RgbQuad * [fileInfoHeaders[i].biHeight];
        for (unsigned j = 0; j < fileInfoHeaders[i].biHeight; j++)
        {
            blurredRgbInfo[j] = new RgbQuad[fileInfoHeaders[i].biWidth];
        }
        blurredRgbInfos.push_back(blurredRgbInfo);

        unsigned height = fileInfoHeaders[i].biHeight / blocksCount;
        unsigned startingIndex = 0;
        for (unsigned j = 0; j < blocksCount - 1; ++j)
        {
            tasks.push_back(new Blurring(initialRgbInfos[i], blurredRgbInfos[i], fileInfoHeaders[i], startingIndex, height));
            startingIndex += height;
        }

        height = fileInfoHeaders[i].biHeight - startingIndex;
        tasks.push_back(new Blurring(initialRgbInfos[i], blurredRgbInfos[i], fileInfoHeaders[i], startingIndex, height));
    }

    Pool pool(tasks, threadsCount);

    unsigned startTime = timeGetTime();
    pool.ExecuteTasks();
    cout << timeGetTime() - startTime;

    return blurredRgbInfos;
}

int main(int argc, char* argv[])
{
    if (argc < 5 || string(argv[1]) == "1" && argc < 6 || string(argv[1]) != "0" && string(argv[1]) != "1")
    {
        cout << "Usage: " << argv[0] << " 0|1(Processing mode: 0 - ordinary, 1 - pool) blocks_count input_directory output_directory threads_count(If processing mode - 1)" << endl;
        return 1;
    }

    try
    {
        unsigned blocksCount = stringToThreadsCount(argv[2]);
        unsigned threadsCount;
        if (string(argv[1]) == "1")
        {
            threadsCount = stringToThreadsCount(argv[5]);
        }

        WIN32_FIND_DATAA wfd;
        HANDLE const hFind = FindFirstFileA(string(string(argv[3]) + "\\*").data(), &wfd);
        if (INVALID_HANDLE_VALUE != hFind)
        {
            vector<string> fileNames;
            vector<BitmapFileHeader> fileHeaders;
            vector<BitmapInfoHeader> fileInfoHeaders;
            vector<RgbQuad**> rgbInfos;
            do
            {
                string fileName = &wfd.cFileName[0];
                if (fileName.size() >= 4)
                {
                    BitmapFileHeader fileHeader;
                    BitmapInfoHeader fileInfoHeader;
                    RgbQuad** rgbInfo;

                    try
                    {
                        rgbInfo = readBmpFile(string(argv[3]) + "\\" + fileName, fileHeader, fileInfoHeader);

                        fileNames.push_back(fileName);
                        fileHeaders.push_back(fileHeader);
                        fileInfoHeaders.push_back(fileInfoHeader);
                        rgbInfos.push_back(rgbInfo);
                    }
                    catch (const runtime_error& e)
                    {
                        cout << e.what() << endl;
                    }
                }
            } while (FindNextFileA(hFind, &wfd) != NULL);
            FindClose(hFind);

            vector<RgbQuad**> blurredRgbInfos;
            if (string(argv[1]) == "0")
            {
                blurredRgbInfos = blurImages(rgbInfos, fileInfoHeaders, blocksCount);
            }
            else
            {
                blurredRgbInfos = blurImagesWithPool(rgbInfos, fileInfoHeaders, blocksCount, threadsCount);
            }

            for (int i = 0; i < fileHeaders.size(); i++)
            {
                writeBmpFile(string(argv[4]) + "/" + fileNames[i], blurredRgbInfos[i], fileHeaders[i], fileInfoHeaders[i]);
            }
        }
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

    return 0;
}