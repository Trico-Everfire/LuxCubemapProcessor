#include <cmath>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include "cubemap_bsp.h"
#include "KeyValue.h"
#include "zip_handler.h"

#include "VTFFile.h"

#define BSPHeaderIdentifier	(('P'<<24)+('S'<<16)+('B'<<8)+'V')
#define BSPPakFileLocation 40
#define BSPCubeMapLocation 42
#define BSPEntityLocation 0

double GetFloatPrecision(double value, double precision)
{
    return (floor((value * pow(10, precision) + 0.5)) / pow(10, precision));
}

float Remap (float value, float from1, float to1, float from2, float to2) {
    return (value - from1) / (to1 - from1) * (to2 - from2) + from2;
}

bool parseBSPEntitiesToStringList( const std::string &rawEntityLump, std::vector<KeyValueRoot*> &entityList )
{
    std::string entityStringStack;
    bool inQuotes = false;
    int nestCount = 0;
    for ( char character : rawEntityLump )
    {
        entityStringStack += character;
        if ( character == '"' )
        {
            inQuotes = !inQuotes;
            continue;
        }
        if ( !inQuotes && character == '{' )
            nestCount++;
        if ( !inQuotes && character == '}' )
            nestCount--;
        if ( nestCount < 0 )
            return false;

        if ( nestCount == 0 && character == '}' )
        {
            std::string parsedEntity = ( ( R"("entity" )" + entityStringStack ) );

            auto kv = new KeyValueRoot(parsedEntity.c_str());
            entityList.push_back( kv );
            entityStringStack = "";
        }
    }
    return true;
}

CCubeMapBSP::CCubeMapBSP(const std::string& bspPath)
{
    FILE* fl = fopen(bspPath.c_str(), "rb");
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

        if(zipper_ret == CUnZipHandler::Result::ZIPPER_RESULT_ERROR)
            return;

        auto cubeMap = std::find_if(cubeMapFiles.begin(), cubeMapFiles.end(), [&result](CorrectedCubeMap& cubemap){ return cubemap.vtfName == result;});
        if(cubeMap != cubeMapFiles.end())
        {
            auto vtfFile = VTFLib::CVTFFile();



            if(!vtfFile.Load(buf.data(), buf.size(), false))
                return;

            vtfFile.ConvertInPlace(IMAGE_FORMAT_RGBA32323232F);

            uint32_t vtfSize = VTFLib::CVTFFile::ComputeImageSize(vtfFile.GetWidth(), vtfFile.GetHeight(), 1, IMAGE_FORMAT_RGBA32323232F);
            float individual = (vtfFile.GetWidth() * vtfFile.GetHeight());

            for(int i = 0; i < 6; i++)
            {
                auto imageData = reinterpret_cast<float*>(vtfFile.GetData(0, i, 0, 0));

                float red = 0;
                float green = 0;
                float blue = 0;

                for (int j = 0; j < vtfSize / 4; j += 4) {
                    red += Remap(imageData[j], 0, 20.0f, 0, 1);
                    green += Remap(imageData[j + 1], 0, 20.0f, 0, 1);
                    blue += Remap(imageData[j + 2], 0, 20.0f, 0, 1);
                }

                cubeMap->cubeMapValues[i].x = ((red) / individual);
                cubeMap->cubeMapValues[i].y = ((green) / individual);
                cubeMap->cubeMapValues[i].z = ((blue) / individual);

            }

            for(const auto& path : filePath)
                cubeMap->path.append(path);
        }


    } while (zipper_ret == CUnZipHandler::Result::ZIPPER_RESULT_SUCCESS );

    if(zipper_ret != CUnZipHandler::Result::ZIPPER_RESULT_SUCCESS_EOF)
        return;

    auto zipHandler = new CZipHandler(
            nullptr, 0);

    zipHandler->useExistingZipEntries(*unZipHandler);

    delete unZipHandler;


    if(!zipHandler->IsValid())
        return;

    for(auto &cubeMaps : cubeMapFiles)
    {
        std::string vmt{"ACROHS_DATA\n{\n"};

        for(int i = 0; i < 6; i++)
        {
              vmt.append("$face"+ std::to_string(i) + " ");
              vmt.append(std::string ("\"[") + " " + std::to_string(GetFloatPrecision(cubeMaps.cubeMapValues[i].x, 6)) + " " + std::to_string(GetFloatPrecision(cubeMaps.cubeMapValues[i].y, 6)) + " " + std::to_string(GetFloatPrecision(cubeMaps.cubeMapValues[i].z, 6)) + " ]\"\n");
        }

        vmt.push_back('}');
        cubeMaps.vmf = vmt;


        if(!zipHandler->AddBufferedFileToZip((cubeMaps.path + cubeMaps.vmtName).c_str(),
                                             reinterpret_cast<const unsigned char *>(cubeMaps.vmf.data()),
                                             cubeMaps.vmf.size()))
            return;

    }

    std::string vmt{R"("ACROHS_DATA"
    {
      // This VMT is used to determine whether or not our custom data is present.
      // Each cubemap has its own vmt in which we store the ambient color sampled from that cubemap
      // Without this file, the PBR Shader will fall back to regular ambient cube

)"};


    auto entityLump = &bsp->bspLumps[BSPEntityLocation];

    std::string rawEntityData {fileContents + entityLump->contentOffset, static_cast<size_t>(entityLump->contentLength)};

    std::vector<KeyValueRoot*> entityList;

    parseBSPEntitiesToStringList(rawEntityData, entityList);

    auto result = std::find_if(entityList.begin(), entityList.end(), [](KeyValueRoot* entity){
        return strcmp(entity->Get("entity").Get("classname").Value().string, "light_environment") == 0;
    });

    if(result != entityList.end()) {
        for(int i = 0; i < result[0]->Get("entity").ChildCount(); i++)
        {
            vmt.append("$");
            vmt.append(result[0]->Get("entity")[i].Key().string);
            vmt.append("    ");
            vmt.append(result[0]->Get("entity")[i].Value().string);
            vmt.append("\n");
        }
    }

    vmt.push_back('}');

    zipHandler->AddBufferedFileToZip("materials/ACROHS_DATA.vmt", reinterpret_cast<const unsigned char *>(vmt.data()), vmt.size());

    auto iter = entityList.begin();
    while((iter = std::find_if(entityList.begin(), entityList.end(), [](KeyValueRoot* entity){
        return strcmp(entity->Get("entity").Get("classname").Value().string, "lux_data") == 0;
    })) != entityList.end())
    {
        std::string vmt2(R"(ACROHS_DATA)" );
        vmt2.append("\n{\n");

        for(int i = 0; i < (*iter)->Get("entity").ChildCount(); i++)
        {
            vmt2.append("$");
            vmt2.append((*iter)->Get("entity")[i].Key().string);
            vmt2.append("    ");
            vmt2.append((*iter)->Get("entity")[i].Value().string);
            vmt2.append("\n");
        }
        vmt2.append( "\n}");

        zipHandler->AddBufferedFileToZip("materials/lux_data.vmt", reinterpret_cast<const unsigned char *>(vmt2.data()), vmt2.size());

        auto currentIterator = iter;
        iter++;
        entityList.erase(currentIterator);
    }

    std::string newEntityList;
    for(auto entity : entityList)
    {
        newEntityList.append("{\n");
        newEntityList.append(entity->Get("entity").ToString());
        newEntityList.append("}\n");
    }

    if(newEntityList.size() <= entityLump->contentLength + 1){
        memset(fileContents + entityLump->contentOffset, '\0', entityLump->contentLength);
        memcpy(fileContents + entityLump->contentOffset, newEntityList.c_str(), newEntityList.size());
        entityLump->contentLength = newEntityList.size();
    }

    int bufSize = 0;
    std::byte* buff;

    zipHandler->GetZipFile(&(buff), &bufSize);

    delete zipHandler;

    if(bufSize <= 0)
        return;

    auto newBuffer = new std::byte[(size - ContentLump->contentLength) + bufSize];
    auto initialContentLength = ContentLump->contentLength;

    ContentLump->contentLength = bufSize;

    memcpy(newBuffer, fileContents, size - initialContentLength);

    memcpy(newBuffer + ContentLump->contentOffset, buff, bufSize);

    delete[] buff;

    fl = fopen(bspPath.c_str(), "wb");
    if(fl == nullptr)
        return;

    fwrite(newBuffer, 1, (size - initialContentLength) + bufSize, fl );

    fclose( fl );

    delete[] newBuffer;
    delete[] fileContents;

    this->isReady = true;

}
