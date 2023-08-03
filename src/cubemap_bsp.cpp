#include <cmath>
#include <vector>
#include <sstream>
#include <algorithm>
#include "cubemap_bsp.h"
#include "zip_handler.h"
#include "VTFFile.h"

#define BSPHeaderIdentifier	(('P'<<24)+('S'<<16)+('B'<<8)+'V')
#define BSPPakFileLocation 40
#define BSPCubeMapLocation 42

double GetFloatPrecision(double value, double precision)
{
    return (floor((value * pow(10, precision) + 0.5)) / pow(10, precision));
}

CCubeMapBSP::CCubeMapBSP(const std::string& bspPath)
{
    FILE* fl = fopen(bspPath.c_str(), "r");
    if(fl == nullptr)
        return;

    fseek( fl, 0, SEEK_END );

    size_t size = ftell( fl );

    char *fileContents = new char[size];

    rewind( fl );
    fread( fileContents, sizeof( char ), size, fl );

    fclose( fl );

    auto bsp = reinterpret_cast<BSPFileHeader*>(fileContents);

    if(bsp->bspIdentifier != BSPHeaderIdentifier)
        return;

    auto cubeMapLump = bsp->bspLumps[BSPCubeMapLocation];

    if(cubeMapLump.contentLength % sizeof(BSPCubeMapSample) != 0)
        return;

    std::vector<CorrectedCubeMap> cubeMapFiles;
    for(int i = cubeMapLump.contentLength, j = 0; i > 1; i-= sizeof(BSPCubeMapSample), j += sizeof(BSPCubeMapSample))
    {
        auto cubeMap = reinterpret_cast<BSPCubeMapSample*>(fileContents + cubeMapLump.contentOffset + j);
        std::string cubeMapFileName{"c"};
        cubeMapFileName.append(std::to_string(cubeMap->cubeMapOrigin[0]));
        cubeMapFileName.append("_");
        cubeMapFileName.append(std::to_string(cubeMap->cubeMapOrigin[1]));
        cubeMapFileName.append("_");
        cubeMapFileName.append(std::to_string(cubeMap->cubeMapOrigin[2]));

        auto cubeMapVMTName = std::string(cubeMapFileName);

        cubeMapVMTName.append(".vmt");

        cubeMapFileName.append(".hdr.vtf");

        auto correctedCubemap = CorrectedCubeMap();

        correctedCubemap.vtfName = cubeMapFileName;
        correctedCubemap.vmtName = cubeMapVMTName;

        cubeMapFiles.push_back(correctedCubemap);
    }


    auto ContentLump = &bsp->bspLumps[BSPPakFileLocation];

    auto unZipHandler = new CUnZipHandler(reinterpret_cast<std::byte *>(fileContents + ContentLump->contentOffset), ContentLump->contentLength);

    if ( !unZipHandler->IsValid() )
        return;

    CUnZipHandler::Result zipper_ret;
    do
    {
        zipper_ret = CUnZipHandler::Result::ZIPPER_RESULT_SUCCESS;
        std::string zfilename = unZipHandler->GetFileName(nullptr );
        if ( zfilename.empty() )
            return;

        std::istringstream fname(zfilename);
        std::string result;
        std::vector<std::string> filePath;
        while (getline(fname, result, '/'))
        {
            filePath.push_back(result + "/");
        }
        filePath.pop_back(); //we discard the filename.

        if ( unZipHandler->IsDir() )
        {
            ++unZipHandler;
            continue;
        }

        std::vector<std::byte> buf;
        zipper_ret = unZipHandler->Read(buf );


        auto cubeMap = std::find_if(cubeMapFiles.begin(), cubeMapFiles.end(), [&result](CorrectedCubeMap& cubemap){ return cubemap.vtfName == result;});
        if(cubeMap != cubeMapFiles.end())
        {
            auto vtfFile = VTFLib::CVTFFile();

            vtfFile.Load(buf.data(), buf.size(), false);
            for(int i = 0; i < 6; i++)
            {
                vtfFile.ConvertInPlace(IMAGE_FORMAT_RGBA32323232F);
                int vtfSize = VTFLib::CVTFFile::ComputeImageSize(vtfFile.GetWidth(), vtfFile.GetHeight(), 1, IMAGE_FORMAT_RGBA32323232F);
                float red = 0;
                float green = 0;
                float blue = 0;
                auto imageData = reinterpret_cast<float*>(vtfFile.GetData(0, i, 0, 0));
                for(int j = 0; j < vtfSize; j += 4)
                {
                    red += imageData[j];
                    green += imageData[j + 1];
                    blue += imageData[j + 2];
                }
                float individual = static_cast<float>(vtfSize) / 4;
                cubeMap->cubeMapValues[i].x = red / individual;
                cubeMap->cubeMapValues[i].y = green / individual;
                cubeMap->cubeMapValues[i].z = blue / individual;
            }

            for(const auto& path : filePath)
                cubeMap->path.append(path);
        }


    } while (zipper_ret == CUnZipHandler::Result::ZIPPER_RESULT_SUCCESS );

    delete unZipHandler;

    auto zipHandler = new CZipHandler(reinterpret_cast<std::byte *>(fileContents + ContentLump->contentOffset), ContentLump->contentLength);

    if(!zipHandler->IsValid())
        return;

    for(auto &cubeMaps : cubeMapFiles)
    {
        std::string vmt{"ENVMAPDATA\n{\n"};

        for(int i = 0; i < 6; i++)
        {
              vmt.append("$face"+ std::to_string(i) + " ");
              vmt.append(std::string ("\"[") + " " + std::to_string(GetFloatPrecision(cubeMaps.cubeMapValues[i].x, 6)) + " " + std::to_string(GetFloatPrecision(cubeMaps.cubeMapValues[i].y, 6)) + " " + std::to_string(GetFloatPrecision(cubeMaps.cubeMapValues[i].z, 6)) + " ]\"\n");
        }

        vmt.push_back('}');
        cubeMaps.vmf = vmt;

        if(!zipHandler->zipper_add_buf((cubeMaps.path + cubeMaps.vmtName).c_str(),
                                       reinterpret_cast<const unsigned char *>(cubeMaps.vmf.data()), cubeMaps.vmf.size()))
            return;

    }

    int bufSize = 0;
    std::byte* buff;

    zipHandler->GetZipFile(&(buff), &bufSize);

    delete zipHandler;

    auto newBuffer = new std::byte[(size - ContentLump->contentLength) + bufSize];
    auto initialContentLength = ContentLump->contentLength;

    ContentLump->contentLength = bufSize;

    memcpy(newBuffer, fileContents, size - initialContentLength);

    memcpy(newBuffer + ContentLump->contentOffset, buff, bufSize);

    delete[] buff;

    fl = fopen(bspPath.c_str(), "w");
    if(fl == nullptr)
        return;

    fwrite(newBuffer, 1, (size - initialContentLength) + bufSize, fl );

    fclose( fl );

    delete[] newBuffer;
    delete[] fileContents;

    this->isReady = true;

}
