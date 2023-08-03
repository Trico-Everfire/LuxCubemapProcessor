#pragma once

#include <string>

class CCubeMapBSP {

#pragma pack(1)

    struct LumpStructure
    {
        int contentOffset;
        int contentLength;
        int lumpVersion;
        char Identifier[4];
    };

    struct BSPFileHeader
    {
        int bspIdentifier;
        int bspVersion;
        LumpStructure bspLumps[64];
        int bspMapRevision;
    };

    struct BSPCubeMapSample
    {
        int cubeMapOrigin[3];
        int cubeMapSize;
    };

    struct LuxVec3
    {
        float x;
        float y;
        float z;
    };

    struct CorrectedCubeMap
    {
        std::string vmtName;
        std::string vtfName;
        std::string path;
        LuxVec3 cubeMapValues[6];
        std::string vmf;
    };

#pragma pack()

    bool isReady = false;

public:
    CCubeMapBSP( const std::string& bspPath );
    bool Available() const {return isReady;}
};
