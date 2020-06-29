#pragma once

#include <list>
#include <string>
#include <vector>

enum eError;

class CArk
{
public:
    CArk();
    ~CArk();

    eError ConstructFromDirectory( const char* lpInputDirectory, const CArk& lReferenceHeader );
    eError BuildArk( const char* lpInputDirectory );
    eError SaveArk( const char* lpOutputDirectory, const char* lpHeaderFilename ) const;

    eError Load( const char* lpHeaderFilename );
    eError ExtractFiles( int liFirstFileIndex, int liNumFiles, const char* lpTargetDirectory );
    bool FileExists( const char* lpFilename ) const;

    int GetNumFiles() const;

private:
    struct sHeaderBase {
        unsigned int muVersion;
        char mChecksumData[ 20 ];
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

    eError LoadArkData();

    const sFileDefinition* GetFile( size_t lFilenameHash, int liDuplicateIndex ) const;

    int miNumArks = 0;
    int miNumFiles = 0;

    sArkDefinition* mpArks = nullptr;
    sFileDefinition* mpFiles = nullptr;
    
    char* mpArkData = nullptr;
};
