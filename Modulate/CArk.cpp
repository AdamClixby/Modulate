#include "pch.h"
#include <direct.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>

#include "CArk.h"

#include "Error.h"
#include "Settings.h"
#include "Utils.h"
#include "CEncryptionCycler.h"

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
            }
        }
    }

    return liNumEntries;
}

eError CArk::ConstructFromDirectory( const char* lpInputDirectory, const CArk& lReferenceHeader )
{
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
    int liNumDuplicates = lReferenceHeader.GetNumberOfFilesIncludingDuplicates( laFilenames ) - miNumFiles;
    mpFiles = new sFileDefinition[ miNumFiles + liNumDuplicates + liNumFakes ];

    unsigned int luTotalFileSize = 0;

    int liFlagToInsert = -1;

    sFileDefinition* lpFileDef = mpFiles;
    std::vector<std::string>::const_iterator lFilename = laFilenames.begin();
    for( int ii = 0; ii < miNumFiles; ++ii, ++lFilename )
    {
        size_t lFilenameHash = std::hash< std::string >{}( *lFilename );

        int jj = 0;
        const sFileDefinition* lpReferenceFile = lReferenceHeader.GetFile( lFilenameHash, jj++ );
        while( lpReferenceFile )
        {
            *lpFileDef = *lpReferenceFile;

            //if( lpFileDef - mpFiles < 226 ) // <226 works <227 fails
            //if( lpFileDef->miFlags1 == -1 )
            //    lpFileDef->miFlags1 = 2147483648;// -16777216;    //ff000000

            //if( lpFileDef - mpFiles == 226 )
            //    lpFileDef->miFlags1 = 0;

            lpReferenceFile = lReferenceHeader.GetFile( lFilenameHash, jj++ );

            if( strstr( lpFileDef->mName.c_str(), "/config/arkbuild/" ) ||
                ( strstr( lpFileDef->mName.c_str(), ".moggsong" ) && !strstr( lpFileDef->mName.c_str(), ".moggsong_dta_ps3" ) ) )
            {
                lpFileDef->miSize = 0;
                ++lpFileDef;
                continue;
            }

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
            lpFileDef->miSize = ftell( lFile );
            fclose( lFile );

            luTotalFileSize += lpFileDef->miSize;

            ++lpFileDef;
        }
    }

    miNumFiles += liNumDuplicates;

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

    miNumArks = 1;
    mpArks = new sArkDefinition[ 1 ];
    mpArks->mPath = lReferenceHeader.mpArks[ 0 ].mPath;

    return eError_NoError;
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
    const unsigned int kuEncryptedVersion = 0xc64eed30;
    if( luVersion != kuEncryptedVersion )
    {
        delete[] lpHeaderData;
        return eError_UnknownVersionNumber;
    }

    CEncryptionCycler lDecrypt;
    lDecrypt.Cycle( lpHeaderData + sizeof( unsigned int ), liHeaderSize - sizeof( unsigned int ), *(int*)lpHeaderData );

    unsigned char* lpHeaderPtr = lpHeaderData + sizeof( unsigned int );
    const sHeaderBase* lpHeaderBase = (const sHeaderBase*)lpHeaderPtr;

    miNumArks = lpHeaderBase->miNumArks;
    if( miNumArks < 0 || miNumArks > 100 )
    {
        return eError_ValueOutOfBounds;
    }

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
        delete[] lpHeaderData;
        return eError_InvalidData;
    }

    lpHeaderPtr += sizeof( int );   // Skip ark strings

    miNumFiles = *(int*)lpHeaderPtr;
    lpHeaderPtr += sizeof( int );

    if( miNumFiles < 0 || miNumFiles > 25000 )
    {
        return eError_ValueOutOfBounds;
    }

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
            eError leError = eError_FailedToOpenFile;
            SHOW_ERROR_AND_RETURN;
        }

        fwrite( &mpArkData[ lpFileDef->mi64Offset ], lpFileDef->miSize, 1, lpOutputFile );
        fclose( lpOutputFile );
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

    return eError_NoError;
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
    WriteUInt( 0 );      // Don't need to save hashes
}

eError CArk::LoadArkData()
{
    if( mpArkData )
    {
        delete[] mpArkData;
    }

    unsigned int luTotalArkSize = 0;
    const sArkDefinition* lpArkDef = mpArks;
    for( int ii = 0; ii < miNumArks; ++ii, ++lpArkDef )
    {
        luTotalArkSize += lpArkDef->muSize;
    }

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

eError CArk::BuildArk( const char* lpInputDirectory )
{
    if( mpArkData )
    {
        delete[] mpArkData;
    }

    VERBOSE_OUT( "Building ark\n" );

    unsigned int luTotalArkSize = 0;
    sFileDefinition* lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    {
        luTotalArkSize += lpFileDef->miSize;
    }

    luTotalArkSize++;

    mpArkData = new char[ luTotalArkSize ];
    char* lpArkPtr = mpArkData + 1;

    lpFileDef = mpFiles;
    for( int ii = 0; ii < miNumFiles; ++ii,++lpFileDef )
    
    //lpFileDef = mpFiles + miNumFiles - 1;
    //for( int ii = miNumFiles - 1; ii >= 0; --ii, --lpFileDef )
    //
    //std::vector< sFileDefinition* > lapFiles;
    //lapFiles.reserve( miNumFiles );

    //lpFileDef = mpFiles;
    //for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
    //{
    //    lapFiles.push_back( lpFileDef );
    //}

    //while( !lapFiles.empty() )
    {
        //int liEntry = rand() % lapFiles.size();
        //lpFileDef = lapFiles[ liEntry ];
        //lapFiles.erase( lapFiles.begin() + liEntry );

        if( lpFileDef->miSize == 0 )
        {
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

        lpFileDef->mi64Offset = ( lpArkPtr - mpArkData );
        fread( lpArkPtr, lpFileDef->miSize, 1, lpInputFile );
        fclose( lpInputFile );

        lpArkPtr += lpFileDef->miSize;
    }

    mpArks[ 0 ].muSize = luTotalArkSize;

    VERBOSE_OUT( "Ark built\n" );
    return eError_NoError;
}

eError CArk::SaveArk( const char* lpOutputDirectory, const char* lpHeaderFilename ) const
{
    auto lSaveArk = [&]()
    {
        const char* lpArkPtr = mpArkData;
        const sArkDefinition* lpArkDef = mpArks;
        for( int ii = 0; ii < miNumArks; ++ii )
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
                    VERBOSE_OUT( "Output file already exists, skipping: " << lFilename.c_str() << "\n" );
                    continue;
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
            VERBOSE_OUT( lFilename.c_str() << "\n" );

            fopen_s( &lpOutputFile, lFilename.c_str(), "wb" );
            if( lpOutputFile )
            {
                fwrite( lpArkPtr, lpArkDef->muSize, 1, lpOutputFile );
                fclose( lpOutputFile );

                lpArkPtr += lpArkDef->muSize;
            }
            else
            {
                VERBOSE_OUT( "Failed to open file for writing: " << lFilename.c_str() << "\n" );
            }
        }
    };

    auto lSaveHeader = [ & ]()
    {
        const unsigned int kuUnencryptedVersion = 9;
        FILE* lpHeaderFile = nullptr;
        fopen_s( &lpHeaderFile, lpHeaderFilename, "rb" );
        if( !lpHeaderFile )
        {
            return;
        }

        const int kiMaxHeaderSize = 512 * 1024;
        unsigned char lacHeaderData[ kiMaxHeaderSize ];

        const unsigned int kuEncryptedVersion = 0xc64eed30;
        *(unsigned int*)( lacHeaderData ) = kuEncryptedVersion;

        unsigned char* lpHeaderPtr = lacHeaderData + ( 1 * sizeof( unsigned int ) );
        sHeaderBase* lpHeaderBase = (sHeaderBase*)lpHeaderPtr;
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
        lpArkChecksums->SetValue( 0, 0 );
        lpHeaderPtr += lpArkChecksums->GetDataSize();

        sIntList* lpStringCounts = (sIntList*)lpHeaderPtr;
        lpStringCounts->miNum = miNumArks;
        lpStringCounts->SetValue( 0, 0 );
        lpHeaderPtr += lpStringCounts->GetDataSize();

        *(int*)lpHeaderPtr = miNumFiles;
        lpHeaderPtr += sizeof( int );

        const sFileDefinition* lpFile = mpFiles;
        for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
        {
            lpFile->Serialise( lpHeaderPtr );
        }

        sIntList* lpFileFlags2 = (sIntList*)lpHeaderPtr;
        lpFileFlags2->miNum = miNumFiles;
    
        lpFile = mpFiles;
        for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
        {
            lpFileFlags2->SetValue( ii, lpFile->miFlags2 );
        }

        lpHeaderPtr += lpFileFlags2->GetDataSize();
        
        //lpFile = mpFiles;
        //for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
        //{
        //    std::cout << ii << ":\t" << lpFile->miFlags1 << "\t" << lpFile->miFlags2 << "\t" << lpFile->mName.c_str() << "\n";
        //}
        //std::cout << "\n\n";

        //{
        //    int liVal = -1;
        //    for( ; liVal < miNumFiles; )
        //    {
        //        lpFile = mpFiles;
        //        for( int ii = 0; ii < miNumFiles; ++ii, ++lpFile )
        //        {
        //            if( (liVal == -1 && lpFile->miFlags1 == liVal && lpFile->miFlags2 == liVal ) ||
        //                ( liVal != -1 && ( lpFile->miFlags1 == liVal || lpFile->miFlags2 == liVal ) ) )
        //            {
        //                std::cout << "\t" << ii << "\t" << lpFile->miFlags1 << "\t" << lpFile->miFlags2 << "\t" << lpFile->mName.c_str() << "\n";
        //            }
        //        }
        //        ++liVal;
        //    }
        //}
        //std::cout << "\n\n";

        //int liNumOutput = 0;
        //int liPreviousLowestFlags = -10000;
        //for( ; liNumOutput < miNumFiles; )
        //{
        //    int liLowestFlags = INT_MAX;
        //    for( int jj = 0; jj < miNumFiles; ++jj )
        //    {
        //        const sFileDefinition* lpFileDef = &mpFiles[ jj ];
        //        if( lpFileDef->miFlags1 < liLowestFlags &&
        //            lpFileDef->miFlags1 > liPreviousLowestFlags )
        //        {
        //            liLowestFlags = lpFileDef->miFlags1;
        //        }
        //    }
        //    for( int jj = 0; jj < miNumFiles; ++jj )
        //    {
        //        const sFileDefinition* lpFileDef = &mpFiles[ jj ];
        //        if( lpFileDef->miFlags1 == liLowestFlags )
        //        {
        //            ++liNumOutput;
        //            std::cout << jj << ":\t"  << lpFileDef->miFlags1 << "\t" << lpFileDef->miFlags2 << "\t" << lpFileDef->mName.c_str() << "\n";
        //        }
        //    }
        //    liPreviousLowestFlags = liLowestFlags;
        //}

        //std::cout << "\n\n";

        //liNumOutput = 0;
        //liPreviousLowestFlags = -10000;
        //for( ; liNumOutput < miNumFiles; )
        //{
        //    int liLowestFlags = INT_MAX;
        //    for( int jj = 0; jj < miNumFiles; ++jj )
        //    {
        //        const sFileDefinition* lpFileDef = &mpFiles[ jj ];
        //        if( lpFileDef->miFlags2 < liLowestFlags &&
        //            lpFileDef->miFlags2 > liPreviousLowestFlags )
        //        {
        //            liLowestFlags = lpFileDef->miFlags2;
        //        }
        //    }
        //    for( int jj = 0; jj < miNumFiles; ++jj )
        //    {
        //        const sFileDefinition* lpFileDef = &mpFiles[ jj ];
        //        if( lpFileDef->miFlags2 == liLowestFlags )
        //        {
        //            ++liNumOutput;
        //            std::cout << jj << ":\t" << lpFileDef->miFlags1 << "\t" << lpFileDef->miFlags2 << "\t" << lpFileDef->miSize << "\t" << lpFileDef->mName.c_str() << "\n";
        //        }
        //    }
        //    liPreviousLowestFlags = liLowestFlags;
        //}

        int liHeaderDataSize = (int)( lpHeaderPtr - lacHeaderData );

        CEncryptionCycler lDecrypt;
        lDecrypt.Cycle( lacHeaderData + sizeof( unsigned int ), liHeaderDataSize - sizeof( unsigned int ), -967906000 );

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
            fwrite( lacHeaderData, liHeaderDataSize, 1, lpOutputFile );
            fclose( lpOutputFile );
        }
        else
        {
            VERBOSE_OUT( "Failed to open file for writing: " << lHeaderFilename.c_str() << "\n" );
        }
    };

    lSaveHeader();
    lSaveArk();

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
