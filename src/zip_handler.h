#pragma once

#include <string>
#include <vector>
extern "C" {
#include "mz_compat.h"
#include "mz_zip.h"
#include "mz_strm.h"
#include "mz_zip_rw.h"
#include "mz_strm_os.h"
#include "mz_os.h"
}
#include <unordered_map>



class CUnZipHandler
{
private:
    static constexpr int BUF_SIZE = 8192;
    static constexpr int MAX_NAMELEN = 256;
    bool isValid = true;
    unzFile zfile;

public:
    enum class Result
    {
        ZIPPER_RESULT_ERROR = 0,
        ZIPPER_RESULT_SUCCESS,
        ZIPPER_RESULT_SUCCESS_EOF
    };

    int operator++(){
        return unzGoToNextFile( zfile );
    };

    ~CUnZipHandler(){
        unzClose( zfile );
    }

    bool IsValid(){
        return isValid;
    }

    CUnZipHandler(std::byte*, int32_t size);
    Result Read( std::vector<std::byte> &fileContents );
    bool SkipFile();
    bool IsDir();
    uint64_t GetFileSize();

    std::string GetFileName( bool *isUTF8 );
};

class CZipHandler
{

bool isValid = false;

uint32_t table[256];

 std::string globalComment;

#pragma pack(1)
struct LocalFileHeader
{
    int signature = 0x04034b50;
    short version = 10;
    short flags = 0;
    short compressionMethod = 0;
    short modificationTime = 0;
    short modificationDate = 0;
    unsigned int crc32;
    int compressionSize;
    int uncompressedSize;
    short fileNameLength;
    short extraFieldLength = 0;
};

struct CentralDirectory
{
    int signature = 0x02014b50;
    short versionMadeBy = 819;
    short version = 20;
    short flags = 8;
    short compressionMethod = 0;
    short modificationTime = 0;
    short modificationDate = 0;
    unsigned int crc32;
    int compressionSize;
    int uncompressedSize;
    short fileNameLength;
    short extraFieldLength = 0;
    short commentLength;
    short diskNumber;
    short internalFileAttributes;
    int externalFileAttributes;
    unsigned int RelativeOffsetLocalFileHeader;


};

struct ZipEntryContents
{
    LocalFileHeader localHeader;
    CentralDirectory centralDirectory;
    std::string fileName;
    std::vector<std::byte> fileData = {};
    uint32_t contentSize;
    std::vector<std::byte> localExtraFieldData;
    std::vector<std::byte> centralExtraFieldData;
    std::string centralComment;
//    short extraFieldLength = 0;
//    short commentLength;
};

struct EndOfCentralDirectory
{
    int signature = 0x06054b50;
    short numberOfThisDisk;
    short diskWhereCentralDirectoryStarts;
    short numberOfCentralDirectoryRecordsOnDisk;
    short totalNumberOfCentralRecords;
    int sizeOfCentralDirectory;
    int offsetOfStartCentralDirectory;
    short commentLength;


};
#pragma pack()

std::vector<ZipEntryContents> entries;
//std::vector<CentralDirectory> centralDirectories;

public:
    CZipHandler(std::byte* buff, uint32_t size);

    ~CZipHandler()
    {
//        for(auto pair : entries)
//            delete pair.second;
    }
    bool IsValid(){
        return isValid;
    }

    bool zipper_add_file(const char *filepath, const char *inZipName);
    bool zipper_add_buf(const char *zfilename, const unsigned char *buf, size_t buflen);

    void GetZipFile(std::byte **buff, int *size);
};