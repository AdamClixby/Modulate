#include "pch.h"
#include <direct.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <windows.h>

#include "CArk.h"

#include "assert.h"

#include "Error.h"
#include "Settings.h"
#include "Utils.h"
#include "CEncryptionCycler.h"

const unsigned int kuMaxArkSize = 1024 * 1024 * 512;

CArk::CArk()
{
}

CArk::~CArk()
{
    delete[] mpArks;
    mpArks = nullptr;

    delete[] mpFiles;
    mpFiles = nullptr;

    delete[] mpArkData;
    mpArkData = nullptr;
}

int CArk::GetNumberOfFilesIncludingDuplicates( const std::vector<std::string>& laFilenames ) const
{
    int liNumEntries = 0;
    for( std::vector<std::string>::const_iterator lIter = laFilenames.begin(); lIter != laFilenames.end(); ++lIter )
    {
        size_t lFilenameHash = std::hash< std::string >{}( *lIter );
        
        const sFileDefinition* lpFileDef = mpFiles;
        for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
        {
            if( lFilenameHash == lpFileDef->mNameHash )
            {
                ++liNumEntries;
                break;
            }
        }
    }

    return liNumEntries;
}

bool CArk::ShouldPackFile( const std::vector< SSongConfig >& laSongs, const char* lpFilename )
{
    bool lbIsValidFile = true;
    if( const char* lpSongName = strstr( lpFilename, "/songs/" ) )
    {
        lbIsValidFile = false;

        // Check that this song is in the song list
        static const size_t liSongsPathLength = strlen( "/songs/" );

        const char* lpSongNameEnd = lpSongName + liSongsPathLength;
        while( *lpSongNameEnd && *lpSongNameEnd != '/' )
        {
            ++lpSongNameEnd;
        }

        if( *lpSongNameEnd )
        {
            std::string lSongName( lpSongName, lpSongNameEnd - lpSongName );
            std::transform( lSongName.begin(), lSongName.end(), lSongName.begin(), []( unsigned char c ) {
                return std::tolower( c );
            } );

            for( const SSongConfig& lSong : laSongs )
            {
                if( strstr( lSong.mPath.c_str(), lSongName.c_str() ) != 0 )
                {
                    lbIsValidFile = true;
                    break;
                }
            }
        }
    }
    return lbIsValidFile;
};

eError CArk::ConstructFromDirectory( const char* lpInputDirectory, const CArk& lReferenceHeader, std::vector< SSongConfig > laSongs )
{
    laSongs.push_back( { "", "", "", "", "/songs/credits", "", -1 } );
    laSongs.push_back( { "", "", "", "", "/songs/tut0",    "", -1 } );
    laSongs.push_back( { "", "", "", "", "/songs/tut1",    "", -1 } );
    laSongs.push_back( { "", "", "", "", "/songs/tutc",    "", -1 } );

    std::vector<std::string> laFilenames;
    laFilenames.reserve( lReferenceHeader.miNumFiles );

    miNumFiles = CUtils::GenerateFileList( lReferenceHeader, lpInputDirectory, laFilenames );
    if( miNumFiles <= 0 )
    {
        eError leError = eError_NoData;
        SHOW_ERROR_AND_RETURN;
    }

    VERBOSE_OUT( "Found " << miNumFiles << " files\n" );
    
    int liNumFakes = 1;
    int liNumDuplicates = CSettings::mbIgnoreNewFiles ? ( lReferenceHeader.GetNumberOfFilesIncludingDuplicates( laFilenames ) - miNumFiles ) : 0;
    
    //if( miNumFiles < lReferenceHeader.miNumFiles )
    //{
    //    miNumFiles = lReferenceHeader.miNumFiles;
    //}

    delete[] mpFiles;
    mpFiles = new sFileDefinition[ miNumFiles + liNumDuplicates + liNumFakes ];

    unsigned __int64 luTotalFileSize = 0;

    sFileDefinition* lpFileDef = mpFiles;

    int liNumFilesIgnored = 0;
    std::vector<std::string>::const_iterator lFilename = laFilenames.begin();
    for( int ii = 0; ii < miNumFiles && lFilename != laFilenames.end(); ++ii, ++lFilename )
    {
        size_t lFilenameHash = std::hash< std::string >{}( *lFilename );

        int jj = 0;
        const sFileDefinition* lpReferenceFile = lReferenceHeader.GetFile( lFilenameHash, jj++ );
        if( !lpReferenceFile && !CSettings::mbIgnoreNewFiles )
        {
            FILE* lFile = nullptr;
            std::string lSourceFilename = lpInputDirectory + *lFilename;
            fopen_s( &lFile, lSourceFilename.c_str(), "rb" );
            if( !lFile )
            {
                std::cout << "Unable to open file: " << (*lFilename).c_str() << "\n";
                ++lpFileDef;
                continue;
            }

            if( ShouldPackFile( laSongs, lFilename->c_str() ) )
            {
                lpFileDef->mName = *lFilename;
                fseek( lFile, 0, SEEK_END );
                int liNewSize = ftell( lFile );
                //assert( liNewSize == lpFileDef->miSize );
                lpFileDef->miSize = liNewSize;
                fclose( lFile );

                lpFileDef->CalculateHashesAndPath();

                luTotalFileSize += lpFileDef->miSize;
                ++lpFileDef;
            }
            else
            {
                fclose( lFile );
                ++liNumFilesIgnored;
            }
        }
        else
        {
            if( lpReferenceFile )
            {
                *lpFileDef = *lpReferenceFile;
                lpReferenceFile = lReferenceHeader.GetFile( lFilenameHash, jj++ );

                if( ShouldPackFile( laSongs, lpFileDef->mName.c_str() ) )
                {
                    FILE* lFile = nullptr;
                    std::string lSourceFilename = lpInputDirectory + lpFileDef->mName;
                    fopen_s( &lFile, lSourceFilename.c_str(), "rb" );
                    if( !lFile )
                    {
                        std::cout << "Unable to open file: " << lSourceFilename.c_str() << "\n";
                        ++lpFileDef;
                        continue;
                    }

                    fseek( lFile, 0, SEEK_END );
                    int liNewSize = ftell( lFile );
                    //assert( liNewSize == lpFileDef->miSize );
                    lpFileDef->miSize = liNewSize;
                    fclose( lFile );

                    luTotalFileSize += lpFileDef->miSize;
                    ++lpFileDef;
                }
                else
                {
                    ++liNumFilesIgnored;
                }
            }
        }
    }

    miNumFiles += liNumDuplicates - liNumFilesIgnored;

    miNumArks = lReferenceHeader.miNumArks;

    delete[] mpArks;
    mpArks = new sArkDefinition[ miNumArks ];

    unsigned __int64 luSizeRemaining = luTotalFileSize;
    for( int ii = 0; ii < miNumArks; ++ii )
    {
        mpArks[ ii ] = lReferenceHeader.mpArks[ ii ];
        mpArks[ ii ].muSize = (unsigned int)( luSizeRemaining / ( miNumArks - ii ) );
        luSizeRemaining -= mpArks[ ii ].muSize;
    }

    return eError_NoError;
}

void CArk::SortFiles()
{
    auto lFileDefinitionSort = []( const void* a, const void* b )
    {
        const sFileDefinition* lpA = (const sFileDefinition*)a;
        const sFileDefinition* lpB = (const sFileDefinition*)b;

        if( lpA->mNameHash == 0 )
        {
            if( lpB->mNameHash == 0 )
            {
                return 0;
            }
            return 1;
        }
        else if( lpB->mNameHash == 0 )
        {
            return -1;
        }

        int liAIndex = 0;
        int liBIndex = 0;

        int liLoopLimit = 16;
        while( --liLoopLimit )
        {
            if( liAIndex + 1 == lpA->mPathBreakdown.size() )
            {
                if( liBIndex + 1 != lpB->mPathBreakdown.size() )
                {
                    return -1;
                }
            }
            else if( liBIndex + 1 == lpB->mPathBreakdown.size() )
            {
                return 1;
            }

            const std::string& lpAName = lpA->mPathBreakdown[ liAIndex ];
            const std::string& lpBName = lpB->mPathBreakdown[ liBIndex ];
            int liComparisonResult = _stricmp( lpAName.c_str(), lpBName.c_str() );
            if( liComparisonResult != 0 )
            {
                return liComparisonResult > 0 ? 1 : -1;
            }

            ++liAIndex;
            ++liBIndex;

            if( liAIndex == lpA->mPathBreakdown.size() )
            {
                if( lpA->miFlags1 < lpB->miFlags1 )
                {
                    return -1;
                }
                else if( lpA->miFlags1 > lpB->miFlags1 )
                {
                    return 1;
                }

                if( lpA->miFlags2 < lpB->miFlags2 )
                {
                    return -1;
                }
                else if( lpA->miFlags2 > lpB->miFlags2 )
                {
                    return 1;
                }

                return 0;
            }
        }

        return 0;
    };

    std::qsort( mpFiles, miNumFiles, sizeof( sFileDefinition ), lFileDefinitionSort );
}

eError CArk::Load( const char* lpHeaderFilename )
{
    if( mpArks )
    {
        return eError_AlreadyLoaded;
    }

    VERBOSE_OUT( "Loading header file " << lpHeaderFilename );

    const unsigned int kuUnencryptedVersion = 9;
    FILE* lpHeaderFile = nullptr;
    fopen_s( &lpHeaderFile, lpHeaderFilename, "rb" );
    if( !lpHeaderFile )
    {
        return eError_FailedToOpenFile;
    }

    fseek( lpHeaderFile, 0, SEEK_END );
    unsigned int liHeaderSize = ftell( lpHeaderFile );
    unsigned char* lpHeaderData = new unsigned char[ liHeaderSize ];

    fseek( lpHeaderFile, 0, SEEK_SET );
    fread( lpHeaderData, liHeaderSize, 1, lpHeaderFile );
    fclose( lpHeaderFile );

    VERBOSE_OUT( "\nLoaded header (" << liHeaderSize << ") bytes\n" );

    unsigned int luVersion = *(unsigned int*)( lpHeaderData );
    if( luVersion != CSettings::kuEncryptedVersionPS3 &&
        luVersion != CSettings::kuEncryptedVersionPS4 )
    {
        delete[] lpHeaderData;
        return eError_UnknownVersionNumber;
    }

    const unsigned int kuInitialKey = ( luVersion == CSettings::kuEncryptedVersionPS3 ) ? CSettings::kuEncryptedPS3Key : CSettings::kuEncryptedPS4Key;

    CEncryptionCycler lDecrypt;
    lDecrypt.Cycle( lpHeaderData + sizeof( unsigned int ), liHeaderSize - sizeof( unsigned int ), kuInitialKey );

    unsigned char* lpHeaderPtr = lpHeaderData + sizeof( unsigned int );
    const sHeaderBase* lpHeaderBase = (const sHeaderBase*)lpHeaderPtr;

    miNumArks = lpHeaderBase->miNumArks;
    if( miNumArks < 0 || miNumArks > 100 )
    {
        eError leError = eError_ValueOutOfBounds;
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
    }

    delete[] mpArks;
    mpArks = new sArkDefinition[ miNumArks ];

    lpHeaderPtr += sizeof( sHeaderBase );
    const sIntList* lpArkSizes = (const sIntList*)( lpHeaderPtr );

    sArkDefinition* lpArk = mpArks;
    for( int ii = 0; ii < miNumArks; ++ii, ++lpArk )
    {
        int liArkSize = 0;
        eError leError = lpArkSizes->GetValue( ii, liArkSize );
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
        lpArk->muSize = (unsigned int)liArkSize;
    }

    lpHeaderPtr += lpArkSizes->GetDataSize();
    const sStringList* lpArkPaths = (const sStringList*)( lpHeaderPtr );
    
    lpArk = mpArks;
    for( int ii = 0; ii < miNumArks; ++ii, ++lpArk )
    {
        std::string lArkPath;
        eError leError = lpArkPaths->GetValue( ii, lArkPath );
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
        lpArk->mPath = lArkPath;
    }

    lpHeaderPtr += lpArkPaths->GetDataSize();
    const sIntList* lpArkChecksums = (const sIntList*)( lpHeaderPtr );

    lpHeaderPtr += lpArkChecksums->GetDataSize();           // Skip checksums
    lpHeaderPtr += lpArkChecksums->miNum * sizeof( int );   // Skip hashes

    if( *(int*)lpHeaderPtr != 0 )
    {
        eError leError = eError_InvalidData;
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
    }

    lpHeaderPtr += sizeof( int );   // Skip ark strings

    miNumFiles = *(int*)lpHeaderPtr;
    lpHeaderPtr += sizeof( int );

    if( miNumFiles < 0 || miNumFiles > 25000 )
    {
        eError leError = eError_ValueOutOfBounds;
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
    }

    delete[] mpFiles;
    mpFiles = new sFileDefinition[ miNumFiles ];
    sFileDefinition* lpFile = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
    {
        eError leError = lpFile->InitialiseFromData( lpHeaderPtr );
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
    }

    const sIntList* lpFileFlags2 = (const sIntList*)( lpHeaderPtr );
    lpFile = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
    {
        eError leError = lpFileFlags2->GetValue( ii, lpFile->miFlags2 );
        SHOW_ERROR_AND_RETURN_W( delete[] lpHeaderData );
    }

    //SortFiles();

    delete[] lpHeaderData;
    return eError_NoError;
}

eError CArk::ExtractFiles( int liFirstFileIndex, int liNumFiles, const char* lpTargetDirectory )
{
    if( !mpFiles )
    {
        return eError_NoData;
    }

    eError leError = LoadArkData();
    SHOW_ERROR_AND_RETURN;

    sFileDefinition* lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    {
        std::string lOutputPath = lpTargetDirectory + lpFileDef->mName;
        FILE* lpOutputFile = nullptr;
        fopen_s( &lpOutputFile, lOutputPath.c_str(), "rb" );
        if( lpOutputFile )
        {
            int liFileSize = 0;
            if( !CSettings::mbOverwriteOutputFiles )
            {
                fseek( lpOutputFile, 0, SEEK_END );
                liFileSize = ftell( lpOutputFile );
            }
            fclose( lpOutputFile );

            if( liFileSize != 0 )
            {
                VERBOSE_OUT( "Output file already exists, skipping: " << lOutputPath.c_str() << "\n" );
                continue;
            }
             
            VERBOSE_OUT( "Overwriting " );
        }
        else
        {
            VERBOSE_OUT( "Writing " );
        }

        char* lpName = new char[ lOutputPath.length() + 1 ];
        memcpy( lpName, lOutputPath.c_str(), lOutputPath.length() + 1 );

        char* lpDirectoryDelim = strstr( lpName, "/" );
        while( lpDirectoryDelim )
        {
            lpDirectoryDelim[ 0 ] = 0;
            _mkdir( lpName );

            struct stat info;
            if( stat( lpName, &info ) != 0 ||
                ( info.st_mode & S_IFDIR ) == 0 )
            {
                eError leError = eError_FailedToCreateDirectory;
                SHOW_ERROR_AND_RETURN_W( delete[] lpName );
            }

            memcpy( lpName, lOutputPath.c_str(), lOutputPath.length() + 1 );
            lpDirectoryDelim = strstr( lpDirectoryDelim + 1, "/" );
        }
        delete[] lpName;

        VERBOSE_OUT( "file " << lOutputPath.c_str() << "\n" );

        fopen_s( &lpOutputFile, lOutputPath.c_str(), "wb" );
        if( !lpOutputFile )
        {
            std::cout << "Failed to create " << lOutputPath.c_str() << "\n";
        }
        else
        {
            size_t liAmountWritten = fwrite( &mpArkData[ lpFileDef->mi64Offset ], 1, lpFileDef->miSize, lpOutputFile );
            fclose( lpOutputFile );
            if( liAmountWritten != lpFileDef->miSize )
            {
                return eError_FailedToWriteData;
            }
        }
    }

    return eError_NoError;
}

int CArk::GetNumFiles() const
{
    return miNumFiles;
}

eError CArk::sIntList::GetValue( int liIndex, int & liResult ) const
{
    if( liIndex >= 0 && liIndex < miNum )
    {
        liResult = ( *( &maValues + liIndex ) );
        return eError_NoError;
    }

    liResult = 0;
    return eError_ValueOutOfBounds;
}

void CArk::sIntList::SetValue( int liIndex, int liValue )
{
    *( &maValues + liIndex ) = liValue;
}

int CArk::sIntList::GetDataSize() const
{
    return sizeof( miNum ) + ( miNum * sizeof( int ) );
}

eError CArk::sStringList::GetValue( int liIndex, std::string & lResult ) const
{
    if( liIndex >= 0 && liIndex < miNum )
    {
        const char* lpValue = &maValues;
        while( --liIndex >= 0 )
        {
            int liLength = *(int*)lpValue;
            lpValue += sizeof( int ) + liLength;
        }

        const int kiMaxStringLength = 255;
        char lacString[ kiMaxStringLength + 1 ];

        int liLength = *(int*)lpValue;
        if( liLength > kiMaxStringLength )
        {
            liLength = kiMaxStringLength;
        }

        memcpy_s( lacString, kiMaxStringLength, lpValue + sizeof( int ), liLength );
        lacString[ liLength ] = 0;

        lResult = std::string( lacString );
        return eError_NoError;
    }

    lResult = "";
    return eError_ValueOutOfBounds;
}

void CArk::sStringList::SetValue( int liIndex, const std::string& lValue )
{
    if( liIndex >= 0 && liIndex < miNum )
    {
        char* lpValue = &maValues;
        while( --liIndex >= 0 )
        {
            int liLength = *(int*)lpValue;
            lpValue += sizeof( int ) + liLength;
        }

        *(int*)lpValue = (int)lValue.length();
        lpValue += sizeof( int );

        memcpy_s( lpValue, lValue.length(), lValue.c_str(), lValue.length() );
    }
}

int CArk::sStringList::GetDataSize() const
{
    const char* lpValue = &maValues;
    for( int ii = 0; ii < miNum; ++ii )
    {
        int liLength = *(int*)lpValue;
        lpValue += sizeof( int ) + liLength;
    }

    return sizeof( miNum ) + (int)( lpValue - &maValues );
}

eError CArk::sFileDefinition::InitialiseFromData( unsigned char*& lpData )
{
    auto ReadInt64 = [ &lpData ]()
    {
        int64_t li64Val = *(int64_t*)lpData;
        lpData += sizeof( int64_t );
        return li64Val;
    };
    auto ReadInt = [ &lpData ]()
    {
        int liVal = *(int*)lpData;
        lpData += sizeof( int );
        return liVal;
    };
    auto ReadUInt = [ &lpData ]()
    {
        unsigned int luVal = *(unsigned int*)lpData;
        lpData += sizeof( unsigned int );
        return luVal;
    };
    auto ReadString = [ &lpData ]()
    {
        const int kiMaxStringLength = 255;
        char lacString[ kiMaxStringLength + 1 ];

        int liLength = *(int*)lpData;
        lpData += sizeof( int );
        if( liLength > kiMaxStringLength )
        {
            liLength = kiMaxStringLength;
        }

        memcpy_s( lacString, kiMaxStringLength, lpData, liLength );
        lacString[ liLength ] = 0;

        lpData += liLength;

        return std::string( lacString );
    };
    
    mi64Offset = ReadInt64();
    mName      = ReadString();
    miFlags1   = ReadInt();
    miSize     = ReadUInt();
    miHash     = ReadUInt();

    if( mName.length() == 0 ||
        mi64Offset < 0 ||
        miSize < 0 )
    {
        return eError_InvalidData;
    }

    CalculateHashesAndPath();

    return eError_NoError;
}

void CArk::sFileDefinition::CalculateHashesAndPath()
{
    mNameHash = std::hash< std::string >{}( mName );

    int liNameLength = (int)mName.length();
    const char* lpExtension = mName.c_str() + liNameLength - 1;
    while( lpExtension > mName.c_str() && *lpExtension != '.' )
    {
        --lpExtension;
    }
    if( lpExtension != mName.c_str() )
    {
        mExtensionHash = std::hash< std::string >{}( std::string( lpExtension ) );
    }

    char* lpNameCopy = new char[ liNameLength + 1 ];
    memcpy( lpNameCopy, mName.c_str(), liNameLength + 1 );

    char* lpFolderBreak = nullptr;
    char* lpFolder = lpNameCopy;
    while( lpFolderBreak = strstr( lpFolder, "/" ) )
    {
        *lpFolderBreak = 0;
        mPathBreakdown.push_back( std::string( lpFolder ) );

        lpFolder = lpFolderBreak + 1;
    }

    mPathBreakdown.push_back( std::string( lpFolder ) );

    delete[] lpNameCopy;
}

void CArk::sFileDefinition::Serialise( unsigned char *& lpData ) const
{
    auto WriteInt64 = [ &lpData ]( int64_t li64Val )
    {
        *(int64_t*)lpData = li64Val;
        lpData += sizeof( int64_t );
        return li64Val;
    };
    auto WriteInt = [ &lpData ]( int liVal )
    {
        *(int*)lpData = liVal;
        lpData += sizeof( int );
        return liVal;
    };
    auto WriteUInt = [ &lpData ]( unsigned int luVal )
    {
        *(unsigned int*)lpData = luVal;
        lpData += sizeof( unsigned int );
        return luVal;
    };
    auto WriteString = [ &lpData ]( const std::string& lVal )
    {
        *(int*)lpData = (int)lVal.length();
        lpData += sizeof( int );

        memcpy_s( lpData, lVal.length(), lVal.c_str(), lVal.length() );
        lpData += lVal.length();
    };

    WriteInt64( mi64Offset );
    WriteString( mName );
    WriteInt( miFlags1 );
    WriteUInt( miSize );

    unsigned int liToWrite = CSettings::mbPS4 ? 0xDDB682F0 : 0x7D401F60;
    WriteUInt( miSize ? liToWrite : 0 );
}

eError CArk::LoadArkData()
{
    if( mpArkData )
    {
        delete[] mpArkData;
    }

    unsigned __int64 luTotalArkSize = 0;
    const sArkDefinition* lpArkDef = mpArks;
    for( int ii = 0; ii < miNumArks; ++ii, ++lpArkDef )
    {
        luTotalArkSize += lpArkDef->muSize;
    }

    delete[] mpArkData;
    mpArkData = new char[ luTotalArkSize ];
    char* lpArkPtr = mpArkData;
    
    lpArkDef = mpArks;
    for( int ii = 0; ii < miNumArks; ++ii, ++lpArkDef )
    {
        FILE* lpArkFile = nullptr;
        fopen_s( &lpArkFile, lpArkDef->mPath.c_str(), "rb" );
        if( !lpArkFile )
        {
            eError leError = eError_FailedToOpenFile;
            SHOW_ERROR_AND_RETURN;
        }
        fread( lpArkPtr, lpArkDef->muSize, 1, lpArkFile );
        fclose( lpArkFile );

        lpArkPtr += lpArkDef->muSize;
    }

    return eError_NoError;
}

eError CArk::BuildArk( const char* lpInputDirectory, std::vector< SSongConfig > laSongs )
{
    laSongs.push_back( { "", "", "", "", "/songs/credits", "", -1 } );
    laSongs.push_back( { "", "", "", "", "/songs/tut0",    "", -1 } );
    laSongs.push_back( { "", "", "", "", "/songs/tut1",    "", -1 } );
    laSongs.push_back( { "", "", "", "", "/songs/tutc",    "", -1 } );

    VERBOSE_OUT( "Building ark\n" );

    unsigned __int64 luTotalArkSize = 0;
    sFileDefinition* lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    {
        //assert( lpFileDef->miSize > 0 );
        luTotalArkSize += lpFileDef->miSize;
    }

    luTotalArkSize++;

    delete[] mpArkData;
    mpArkData = new char[ luTotalArkSize ];
    char* lpArkPtr = mpArkData;

    int liArkIndex = 0;
    int liTotalSizeAllowed = mpArks[ liArkIndex ].muSize;
    char* lpArkStartPtr = lpArkPtr;

    lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    {
        if( lpFileDef->miSize == 0 )
        {
            lpFileDef->mi64Offset = 0;
            continue;
        }

        std::string lFilename = lpInputDirectory + lpFileDef->mName;

        FILE* lpInputFile = nullptr;
        fopen_s( &lpInputFile, lFilename.c_str(), "rb" );
        if( !lpInputFile )
        {
            eError leError = eError_FailedToOpenFile;
            SHOW_ERROR_AND_RETURN;
        }

        {
            lpFileDef->mi64Offset = ( lpArkPtr - mpArkData );
            fread( lpArkPtr, lpFileDef->miSize, 1, lpInputFile );
            fclose( lpInputFile );

            lpArkPtr += lpFileDef->miSize;
            if( lpArkPtr - lpArkStartPtr > liTotalSizeAllowed )
            {
                unsigned int liArkSize = (unsigned int)( lpArkPtr - lpArkStartPtr );
                mpArks[ liArkIndex ].muSize = liArkSize;

                ++liArkIndex;
                liTotalSizeAllowed += mpArks[ liArkIndex ].muSize - liArkSize;

                lpArkStartPtr = lpArkPtr;
            }
        }
    }
    mpArks[ liArkIndex ].muSize = (unsigned int)( lpArkPtr - lpArkStartPtr );

    VERBOSE_OUT( "Ark built\n" );
    return eError_NoError;
}

eError CArk::SaveArk( const char* lpOutputDirectory, const char* lpHeaderFilename ) const
{
    auto lCalculateFileHash = [&]( const std::string& lFilename )
    {
        int liHash = 0;
        const char* lpChar = lFilename.c_str();
        do
        {
            liHash = ( liHash * 0x7F ) + *lpChar;
            liHash -= ( ( liHash / miNumFiles ) * miNumFiles );
        } while( *(++lpChar) );

        return liHash;
    };

    auto lSaveArk = [&]()
    {
        const char* lpArkPtr = mpArkData;
        const sArkDefinition* lpArkDef = mpArks;
        for( int ii = 0; ii < miNumArks; ++ii, ++lpArkDef )
        {
            std::string lFilename = lpOutputDirectory + lpArkDef->mPath;
            FILE* lpOutputFile = nullptr;
            fopen_s( &lpOutputFile, lFilename.c_str(), "rb" );
            if( lpOutputFile )
            {
                int liFileSize = 0;
                if( !CSettings::mbOverwriteOutputFiles )
                {
                    fseek( lpOutputFile, 0, SEEK_END );
                    liFileSize = ftell( lpOutputFile );
                }
                fclose( lpOutputFile );

                if( liFileSize != 0 )
                {
                    std::cout << "Output file already exists: " << lFilename.c_str() << "\n";
                    continue;
                }
                else
                {
                    std::cout << "Overwriting ";
                }
            }
            else
            {
                std::cout << "Writing ";
            }
            std::cout << lFilename.c_str() << "\n";

            fopen_s( &lpOutputFile, lFilename.c_str(), "wb" );
            if( lpOutputFile )
            {
                size_t liAmountWritten = fwrite( lpArkPtr, 1, lpArkDef->muSize, lpOutputFile );
                fclose( lpOutputFile );

                if( liAmountWritten != lpArkDef->muSize )
                {
                    return eError_FailedToWriteData;
                }

                lpArkPtr += lpArkDef->muSize;
            }
            else
            {
                std::cout << "Failed to open file for writing: " << lFilename.c_str() << "\n";
            }
        }
        return eError_NoError;
    };

    auto lSaveHeader = [ & ]()
    {
        const unsigned int kuUnencryptedVersion = 9;
        FILE* lpHeaderFile = nullptr;
        fopen_s( &lpHeaderFile, lpHeaderFilename, "rb" );
        if( !lpHeaderFile )
        {
            return eError_FailedToOpenFile;
        }

        const int kiMaxHeaderSize = 512 * 1024;
        unsigned char lacHeaderData[ kiMaxHeaderSize ];

        const unsigned int kuEncryptedVersion = CSettings::mbPS4 ? CSettings::kuEncryptedVersionPS4 : CSettings::kuEncryptedVersionPS3;
        *(unsigned int*)( lacHeaderData ) = kuEncryptedVersion;

        unsigned char* lpHeaderPtr = lacHeaderData + ( 1 * sizeof( unsigned int ) );
        sHeaderBase* lpHeaderBase = (sHeaderBase*)lpHeaderPtr;
        lpHeaderBase->miNumChecksums = 1;
        lpHeaderBase->muVersion = kuUnencryptedVersion;
        lpHeaderBase->miNumArks = miNumArks;

        lpHeaderPtr += sizeof( sHeaderBase );

        sIntList* lpArkSizes = (sIntList*)lpHeaderPtr;
        lpArkSizes->miNum = miNumArks;

        const sArkDefinition* lpArk = mpArks;
        for( int ii = 0; ii < miNumArks; ++ii, ++lpArk )
        {
            lpArkSizes->SetValue( ii, lpArk->muSize );
        }

        lpHeaderPtr += lpArkSizes->GetDataSize();

        sStringList* lpArkPaths = (sStringList*)lpHeaderPtr;
        lpArkPaths->miNum = miNumArks;

        lpArk = mpArks;
        for( int ii = 0; ii < miNumArks; ++ii, ++lpArk )
        {
            lpArkPaths->SetValue( ii, lpArk->mPath );
        }

        lpHeaderPtr += lpArkPaths->GetDataSize();

        sIntList* lpArkChecksums = (sIntList*)lpHeaderPtr;
        lpArkChecksums->miNum = miNumArks;
        for( int ii = 0; ii < miNumArks; ++ii )
        {
            lpArkChecksums->SetValue( ii, 0 );
        }
        lpHeaderPtr += lpArkChecksums->GetDataSize();

        sIntList* lpStringCounts = (sIntList*)lpHeaderPtr;
        lpStringCounts->miNum = miNumArks;
        for( int ii = 0; ii < miNumArks; ++ii )
        {
            lpStringCounts->SetValue( ii, 0 );
        }
        lpHeaderPtr += lpStringCounts->GetDataSize();

        *(int*)lpHeaderPtr = miNumFiles;
        lpHeaderPtr += sizeof( int );

        std::vector< std::pair< int, sFileDefinition* > > lHashes;

        sFileDefinition* lpFile = mpFiles;
        for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
        {
            int liHash = lCalculateFileHash( lpFile->mName );
            lHashes.push_back( std::pair< int, sFileDefinition* >( liHash, lpFile ) );
        }

        if( CSettings::mbPS4 )
        {
            std::sort( lHashes.begin(), lHashes.end(), []( auto& lA, auto& lB ) {
                const sFileDefinition* lpA = lA.second;
                const sFileDefinition* lpB = lB.second;

                if( lpA->mNameHash == 0 )
                {
                    if( lpB->mNameHash == 0 )
                    {
                        return true;
                    }
                    return false;
                }
                else if( lpB->mNameHash == 0 )
                {
                    return true;
                }

                int liAIndex = 0;
                int liBIndex = 0;

                int liLoopLimit = 16;
                while( --liLoopLimit )
                {
                    if( liAIndex + 1 == lpA->mPathBreakdown.size() )
                    {
                        if( liBIndex + 1 != lpB->mPathBreakdown.size() )
                        {
                            return true;
                        }
                    }
                    else if( liBIndex + 1 == lpB->mPathBreakdown.size() )
                    {
                        return false;
                    }

                    const std::string& lpAName = lpA->mPathBreakdown[ liAIndex ];
                    const std::string& lpBName = lpB->mPathBreakdown[ liBIndex ];
                    int liComparisonResult = _stricmp( lpAName.c_str(), lpBName.c_str() );
                    if( liComparisonResult != 0 )
                    {
                        return liComparisonResult > 0 ? false : true;
                    }

                    ++liAIndex;
                    ++liBIndex;

                    if( liAIndex == lpA->mPathBreakdown.size() )
                    {
                        if( lpA->miFlags1 < lpB->miFlags1 )
                        {
                            return true;
                        }
                        else if( lpA->miFlags1 > lpB->miFlags1 )
                        {
                            return false;
                        }

                        if( lpA->miFlags2 < lpB->miFlags2 )
                        {
                            return true;
                        }
                        else if( lpA->miFlags2 > lpB->miFlags2 )
                        {
                            return false;
                        }

                        return true;
                    }
                }

                return true;
            } );
        }
        else
        {
            std::sort( lHashes.begin(), lHashes.end(), []( auto& lA, auto& lB ) {
                if( lA.first < lB.first )
                {
                    return true;
                }
                else if( lB.first < lA.first )
                {
                    return false;
                }

                return lA.second < lB.second;
            } );
        }
        
        std::vector< std::pair< int, int > > lHashOffsets;

        {
            int liEntryIndex = 0;
            int liPreviousHashIndex = -1;

            std::vector< std::pair< int, sFileDefinition* > >::iterator lEntry = lHashes.begin();
            while( lEntry != lHashes.end() )
            {
                int liHash = lEntry->first;

                int liFlags = -1;
                for( auto& lIter : lHashOffsets )
                {
                    if( lIter.first == lEntry->first )
                    {
                        liFlags = lIter.second;
                        lIter.second = liEntryIndex;
                        break;
                    }
                }

                if( liPreviousHashIndex != -1 )
                {
                    liFlags = liPreviousHashIndex;
                }
                lEntry->second->miFlags1 = liFlags;

                lEntry->second->Serialise( lpHeaderPtr );
                liPreviousHashIndex = liEntryIndex;

                ++lEntry;
                if( lEntry == lHashes.end() )
                {
                    lHashOffsets.push_back( { liHash, liEntryIndex } );
                    break;
                }

                if( lEntry->first != liHash )
                {
                    lHashOffsets.push_back( { liHash, liEntryIndex } );
                    liPreviousHashIndex = -1;
                }

                ++liEntryIndex;
            }
        }

        *(int*)lpHeaderPtr = miNumFiles;
        lpHeaderPtr += sizeof( int );

        for( int ii = 0; ii < miNumFiles; ++ii )
        {
            int liEntryIndex = -1;
            for( auto lFind : lHashOffsets )
            {
                if( lFind.first == ii )
                {
                    liEntryIndex = lFind.second;
                    break;
                }
            }

            *(int*)lpHeaderPtr = liEntryIndex;
            lpHeaderPtr += sizeof( int );
        }

        int liHeaderDataSize = (int)( lpHeaderPtr - lacHeaderData );

        CEncryptionCycler lDecrypt;
        lDecrypt.Cycle( lacHeaderData + sizeof( unsigned int ), liHeaderDataSize - sizeof( unsigned int ), CSettings::mbPS4 ? CSettings::kuEncryptedPS4Key : CSettings::kuEncryptedPS3Key );

        std::string lHeaderFilename = lpOutputDirectory + std::string( lpHeaderFilename );
   
        FILE* lpOutputFile = nullptr;
        fopen_s( &lpOutputFile, lHeaderFilename.c_str(), "rb" );
        if( lpOutputFile )
        {
            int liFileSize = 0;
            if( !CSettings::mbOverwriteOutputFiles )
            {
                fseek( lpOutputFile, 0, SEEK_END );
                liFileSize = ftell( lpOutputFile );
            }
            fclose( lpOutputFile );

            if( liFileSize != 0 )
            {
                VERBOSE_OUT( "Output file already exists, skipping: " << lHeaderFilename.c_str() << "\n" );
            }
            else
            {
                VERBOSE_OUT( "Overwriting " );
            }
        }
        else
        {
            VERBOSE_OUT( "Writing " );
        }
        VERBOSE_OUT( lHeaderFilename.c_str() << "\n" );

        fopen_s( &lpOutputFile, lHeaderFilename.c_str(), "wb" );
        if( lpOutputFile )
        {
            size_t liAmountWritten = fwrite( lacHeaderData, 1, liHeaderDataSize, lpOutputFile );
            fclose( lpOutputFile );
            if( liAmountWritten != liHeaderDataSize )
            {
                return eError_FailedToWriteData;
            }
        }
        else
        {
            VERBOSE_OUT( "Failed to open file for writing: " << lHeaderFilename.c_str() << "\n" );
        }

        return eError_NoError;
    };

    eError leError = lSaveHeader();
    SHOW_ERROR_AND_RETURN;

    leError = lSaveArk();
    SHOW_ERROR_AND_RETURN;

    return eError_NoError;
}

const CArk::sFileDefinition* CArk::GetFile( size_t lFilenameHash, int liDuplicateIndex ) const
{
    const sFileDefinition* lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    {
        if( lpFileDef->mNameHash == lFilenameHash )
        {
            if( --liDuplicateIndex < 0 )
            {
                return lpFileDef;
            }
        }
    }

    return nullptr;
}

bool CArk::FileExists( const char* lpFilename ) const
{
    size_t liFilenameHash = std::hash< std::string >{}( std::string( lpFilename ) );

    const sFileDefinition* lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    {
        if( lpFileDef->mNameHash == liFilenameHash )
        {
            return true;
        }
    }

    return false;
}
