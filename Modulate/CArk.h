#pragma once

#include <list>
#include <string>
#include <vector>
#include "CDtaFile.h"

enum eError;

class CArk
{
public:
    CArk();
    ~CArk();

    eError ConstructFromDirectory( const char* lpInputDirectory, const CArk& lReferenceHeader, std::vector< SSongConfig > laSongs );
    eError BuildArk( const char* lpInputDirectory, std::vector< SSongConfig > laSongs );
    eError SaveArk( const char* lpOutputDirectory, const char* lpHeaderFilename ) const;

    eError Load( const char* lpHeaderFilename );
    eError ExtractFiles( int liFirstFileIndex, int liNumFiles, const char* lpTargetDirectory );
    bool FileExists( const char* lpFilename ) const;

    int GetNumFiles() const;

private:
    struct sHeaderBase {
        unsigned int muVersion;
        unsigned int miNumChecksums;
        char mChecksumData[ 16 ];
        int miNumArks;
    };
    struct sIntList {
        int miNum;
        int maValues;
        eError GetValue( int liIndex, int& liResult ) const;
        void SetValue( int liIndex, int liValue );
        int GetDataSize() const;
    };
    struct sStringList {
        int miNum;
        char maValues;
        eError GetValue( int liIndex, std::string& lResult ) const;
        void SetValue( int liIndex, const std::string& lValue );
        int GetDataSize() const;
    };

    struct sArkDefinition
    {
        std::string mPath = "";
        unsigned int muSize = 0;
    };

    struct sFileDefinition
    {
        eError InitialiseFromData( unsigned char*& lpData );
        void CalculateHashesAndPath();
        void Serialise( unsigned char*& lpData ) const;

        std::string mName;
        size_t mNameHash = 0;
        size_t mExtensionHash = 0;
        int64_t mi64Offset = 0;
        int miFlags1 = -1;
        int miFlags2 = -1;
        int miHash = 0;
        int miSize = 0;
        std::vector< std::string > mPathBreakdown;
    };

    int GetNumberOfFilesIncludingDuplicates( const std::vector<std::string>& laFilenames ) const;

public:
    eError LoadArkData();

private:
    bool ShouldPackFile( const std::vector< SSongConfig >& laSongs, const char* lpFilename );

    void SortFiles();

    const sFileDefinition* GetFile( size_t lFilenameHash, int liDuplicateIndex ) const;

    int miNumArks = 0;
    int miNumFiles = 0;

    sArkDefinition* mpArks = nullptr;
    sFileDefinition* mpFiles = nullptr;
    
    char* mpArkData = nullptr;
};
