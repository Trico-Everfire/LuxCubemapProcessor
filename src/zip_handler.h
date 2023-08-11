#pragma once

#include <string>
#include <vector>
#include <map>

class CZip
{
protected:
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

    CZip(std::byte* buff, uint32_t size);

    std::map<std::string, ZipEntryContents> entries;

    bool isValid = true;

    std::string globalComment;

public:

    void useExistingZipEntries(const CZip&);

public:
    typedef std::map<std::string, ZipEntryContents>::iterator EntryIterator;
    static constexpr int MAX_ENTRIES = 65535;
};

class CUnZipHandler : public CZip
{

    EntryIterator current;

public:
    enum class Result
    {
        ZIPPER_RESULT_ERROR = 0,
        ZIPPER_RESULT_SUCCESS,
        ZIPPER_RESULT_SUCCESS_EOF
    };

    EntryIterator operator++(){
        return current++;
    };

    ~CUnZipHandler(){
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

class CZipHandler : public CZip
{

uint32_t table[256];


public:
    CZipHandler(std::byte *buff, uint32_t size);

    ~CZipHandler() = default;

    bool IsValid(){
        return isValid;
    }

    bool RemoveFileFromZip(const char *filepath);
    bool AddFileToZip(const char *filepath, const char *inZipName);
    bool AddBufferedFileToZip(const char *zfilename, const unsigned char *buf, size_t buflen);

    void GetZipFile(std::byte **buff, int *size);
};