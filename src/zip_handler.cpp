#include <array>
#include <iostream>
#include <cstring>
#include "crc32.h"
extern "C" {
#include "mz_strm_mem.h"
}
#include "zip_handler.h"

std::string CUnZipHandler::GetFileName(bool *isUTF8) {

        if ( !isValid )
            return {};
        std::array<char, BUF_SIZE> name {};
        unz_file_info64 finfo;
        int ret;

        if ( zfile == nullptr )
            return {};

        ret = unzGetCurrentFileInfo64( zfile, &finfo, name.data(), MAX_NAMELEN, nullptr, 0, nullptr, 0 );

        if ( ret != UNZ_OK )
            return {};

        if ( isUTF8 != nullptr )
            *isUTF8 = ( finfo.flag & ( 1 << 11 ) ) != 0;

        return { name.data() };


}

bool CUnZipHandler::IsDir()
{
    if ( !isValid )
        return false;

    std::array<char, BUF_SIZE> name {};
    unz_file_info64 finfo;
    size_t len;
    int ret;

    if ( zfile == nullptr )
        return false;

    ret = unzGetCurrentFileInfo64( zfile, &finfo, name.data(), MAX_NAMELEN, nullptr, 0, nullptr, 0 );
    if ( ret != UNZ_OK )
        return false;

    len = strlen( name.data() );
    if ( finfo.uncompressed_size == 0 && len > 0 && name[len - 1] == '/' )
        return true;

    return false;
}

CUnZipHandler::Result CUnZipHandler::Read(std::vector<std::byte> &fileContents) {
    if ( !isValid )
        return Result::ZIPPER_RESULT_ERROR;

    if ( zfile == nullptr )
    {
        return Result::ZIPPER_RESULT_ERROR;
    }

    int ret = unzOpenCurrentFile( zfile );
    if ( ret != UNZ_OK )
    {
        return Result::ZIPPER_RESULT_ERROR;
    }

    int red;

    std::vector<std::byte> buffer;
    buffer.reserve(BUF_SIZE);
    while ( ( red = unzReadCurrentFile( zfile, buffer.data(), BUF_SIZE ) ) > 0 )
    {
        for(int i = 0; i < red; i++)
        fileContents.push_back(buffer[i]);//.append(  reinterpret_cast<char *>( tbuf.data() ), red  );
    }

    if ( red < 0 )
    {
        unzCloseCurrentFile( zfile );
        return Result::ZIPPER_RESULT_ERROR;
    }

    unzCloseCurrentFile( zfile );

    if ( unzGoToNextFile( zfile ) != UNZ_OK )
    {
        return Result::ZIPPER_RESULT_SUCCESS_EOF;
    }

    return Result::ZIPPER_RESULT_SUCCESS;
}

bool CUnZipHandler::SkipFile()
{
    if ( !isValid )
        return false;
    return unzGoToNextFile( zfile ) != UNZ_OK;
}

uint64_t CUnZipHandler::GetFileSize()
{
    if ( !isValid )
        return -1;

    unz_file_info64 fInfo;
    int ret;

    if ( zfile == nullptr )
        return 0;

    ret = unzGetCurrentFileInfo64( zfile, &fInfo, nullptr, 0, nullptr, 0, nullptr, 0 );
    if ( ret != UNZ_OK )
        return 0;
    return fInfo.uncompressed_size;
}

CUnZipHandler::CUnZipHandler(std::byte* buff, int32_t size) {

    auto stream = mz_stream_mem_create();

    if (!stream)
        return;

    mz_stream_mem_set_buffer(stream, buff, size);

    int res = mz_stream_mem_open(stream, NULL, MZ_OPEN_MODE_WRITE);

    if(res != MZ_OK)
    {
        std::cout << res;
        return;
    }

    this->zfile = unzOpen_MZ(stream);

        this->isValid = true;

}


CZipHandler::CZipHandler(std::byte* buff, uint32_t size) {

    CCRC32::generate_table(table);

    std::unordered_map<int, ZipEntryContents> existingEntries;

    for(int i = 0; i < size; i++)
    {
        if(*reinterpret_cast<int*>(buff + i) == 0x04034b50)
        {
            ZipEntryContents contents{0};
            auto eocdTest = reinterpret_cast<LocalFileHeader*>(buff + i);
            memcpy(&contents.localHeader,eocdTest, sizeof(LocalFileHeader));
            contents.fileName.reserve(eocdTest->fileNameLength );
            contents.fileName.assign(reinterpret_cast<char*>(buff) + i + sizeof(LocalFileHeader), reinterpret_cast<char*>(buff) + i + sizeof(LocalFileHeader) + eocdTest->fileNameLength);
            int offsetSize = contents.fileName.length() + (i + sizeof(LocalFileHeader));

            if(eocdTest->extraFieldLength > 0) {
                contents.localExtraFieldData.reserve(eocdTest->extraFieldLength);
                contents.localExtraFieldData.assign((std::byte *) buff + offsetSize,
                                                    (std::byte *) buff + offsetSize + eocdTest->extraFieldLength);
            }
            offsetSize += eocdTest->extraFieldLength;
            contents.fileData.reserve(eocdTest->uncompressedSize);
            contents.fileData.assign( (std::byte*)buff + offsetSize, (std::byte*)buff + offsetSize + eocdTest->uncompressedSize);

            contents.contentSize = eocdTest->uncompressedSize;
            existingEntries.insert({i, contents});
            continue;
        }

        if(*reinterpret_cast<int*>(buff + i) == 0x02014b50)
        {
            auto eocdTest = reinterpret_cast<CentralDirectory*>(buff + i);
            auto contents = existingEntries[eocdTest->RelativeOffsetLocalFileHeader];//.extract(eocdTest->RelativeOffsetLocalFileHeader);

            memcpy(&contents.centralDirectory,eocdTest, sizeof(CentralDirectory));

            int offsetSize = contents.fileName.length() + (i + sizeof(LocalFileHeader));

            if(eocdTest->extraFieldLength > 0) {
                contents.centralExtraFieldData.reserve(eocdTest->extraFieldLength);
                contents.localExtraFieldData.assign((std::byte *) buff + offsetSize,
                                                    (std::byte *) buff + offsetSize + eocdTest->extraFieldLength);
            }

            offsetSize += eocdTest->extraFieldLength;
            if(eocdTest->commentLength > 0) {
                contents.centralComment.reserve(eocdTest->extraFieldLength);
                contents.centralComment.assign((char *) buff + offsetSize,
                                               (char *) buff + offsetSize + eocdTest->extraFieldLength);
            }

            entries.push_back(contents);
            continue;
        }
        if(*reinterpret_cast<int*>(buff + i) == 0x06054b50) {
            auto eocdTest = reinterpret_cast<EndOfCentralDirectory *>(buff + i);
            this->globalComment.reserve( eocdTest->commentLength );
            this->globalComment.assign(reinterpret_cast<char*>(buff) + i + sizeof(EndOfCentralDirectory), reinterpret_cast<char*>(buff) + i + sizeof(EndOfCentralDirectory) + eocdTest->commentLength);
            continue;
        }
    }

    this->isValid = true;
}

bool CZipHandler::zipper_add_buf(const char *zfilename, const unsigned char *buf, size_t buflen)
{
    ZipEntryContents entry;

    //local header
    LocalFileHeader header;
    header.compressionMethod = 0;
    header.flags = 8;
    header.version = 10;
    header.crc32 = CCRC32::update(table,0,buf,buflen);
    header.modificationTime = 0;
    header.modificationDate = 0;
    header.compressionSize = buflen;
    header.uncompressedSize = buflen;
    header.fileNameLength = strlen(zfilename);

    //central directory header
    CentralDirectory dir;
    dir.crc32 = CCRC32::update(table,0,buf,buflen);;
    dir.flags = 8;
    dir.uncompressedSize = buflen;
    dir.compressionSize = buflen;
    dir.fileNameLength = strlen(zfilename);;
    dir.commentLength = 0;
    dir.diskNumber = 0;
    dir.internalFileAttributes = 0;
    dir.externalFileAttributes = 0;

    //setting headers in entry
    entry.localHeader = header;
    entry.centralDirectory = dir;

    //internal information
    entry.contentSize = buflen;
    entry.fileData.reserve(buflen);
    entry.fileData.assign( (std::byte*)buf, (std::byte*)buf+buflen);
    entry.fileName = zfilename;

    //push to entries.
    entries.push_back(entry);

    return true;

}

void CZipHandler::GetZipFile(std::byte **buff, int *size)
{
    uint32_t totalSize = 0;
    uint32_t extraFilenameCountUp = 0;
    for(auto& entry : entries) {
        totalSize += entry.fileData.size() + (entry.centralDirectory.fileNameLength) + entry.centralDirectory.commentLength + entry.centralDirectory.extraFieldLength;
        extraFilenameCountUp += (entry.centralDirectory.fileNameLength);
    }
    totalSize += ((sizeof(CentralDirectory) + sizeof(LocalFileHeader)) * entries.size()) + sizeof(EndOfCentralDirectory);
    totalSize += extraFilenameCountUp + this->globalComment.length();

    auto currentBuffer = const_cast<std::byte *>(*buff = new std::byte[totalSize]);

    int offset = 0;
    for(auto &entry : entries)
    {
        entry.centralDirectory.RelativeOffsetLocalFileHeader = offset;
        memcpy(currentBuffer + offset, &entry.localHeader, sizeof(LocalFileHeader));
        offset += sizeof(LocalFileHeader);
        memcpy(currentBuffer + offset, entry.fileName.c_str(), strlen(entry.fileName.c_str()));
        offset += strlen(entry.fileName.c_str());
        if(entry.localHeader.extraFieldLength > 0)
        {
            memcpy(currentBuffer + offset, entry.localExtraFieldData.data(), entry.localHeader.extraFieldLength);
            offset+=entry.localHeader.extraFieldLength;
        }
        memcpy(currentBuffer + offset, entry.fileData.data(),entry.fileData.size());
        offset += entry.fileData.size();



    }

    uint32_t startOfCD = offset;
    uint32_t centralDirectorySize = 0;

    for(auto entry : entries)
    {
        memcpy(currentBuffer + offset, &entry.centralDirectory, sizeof(CentralDirectory));
        offset += sizeof(CentralDirectory);
        centralDirectorySize += sizeof(CentralDirectory);
        memcpy(currentBuffer + offset, entry.fileName.c_str(), entry.fileName.length());
        offset += entry.fileName.length();
        centralDirectorySize += entry.fileName.length();
        if(entry.centralDirectory.extraFieldLength > 0)
        {
            memcpy(currentBuffer + offset, entry.centralExtraFieldData.data(), entry.centralDirectory.extraFieldLength);
            offset+=entry.centralDirectory.extraFieldLength;
        }
        if(entry.centralDirectory.commentLength > 0)
        {
            memcpy(currentBuffer + offset, entry.centralComment.data(), entry.centralComment.length());
            offset+=entry.centralComment.length();
        }
    }

    EndOfCentralDirectory endOfCentralDirectory;
    endOfCentralDirectory.commentLength = this->globalComment.length();
    endOfCentralDirectory.numberOfThisDisk = 0;
    endOfCentralDirectory.diskWhereCentralDirectoryStarts = 0;
    endOfCentralDirectory.numberOfCentralDirectoryRecordsOnDisk = entries.size();
    endOfCentralDirectory.totalNumberOfCentralRecords = entries.size();
    endOfCentralDirectory.sizeOfCentralDirectory = centralDirectorySize;
    endOfCentralDirectory.offsetOfStartCentralDirectory = startOfCD;

    memcpy(currentBuffer + offset, &endOfCentralDirectory, sizeof(EndOfCentralDirectory));
    if(!this->globalComment.empty())
    {
        memcpy(currentBuffer + offset + sizeof(EndOfCentralDirectory), this->globalComment.c_str(), this->globalComment.length());
    }

    *size = totalSize;

}

bool CZipHandler::zipper_add_file(const char *filepath, const char *inZipName) {

    FILE* fl = fopen(filepath, "r");
    if(fl == nullptr)
        return false;

    fseek( fl, 0, SEEK_END );
    size_t size = ftell( fl );
    unsigned char* fileContents = new unsigned char[size];

    rewind( fl );
    fread( fileContents, sizeof( char ), size, fl );

    fclose( fl );

    bool success = zipper_add_buf(inZipName ? inZipName : filepath, fileContents, size);;

    delete[] fileContents;

    return success;
}
