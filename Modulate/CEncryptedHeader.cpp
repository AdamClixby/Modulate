#include "pch.h"
#include "CEncryptedHeader.h"

#include <windows.h>
#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <vector>

int CHeaderData::GetFileArkIndex( int liFileNum, unsigned int& llOffsetInArk ) const
{
    int64_t lu64Offset = maFiles[ liFileNum ].mu64Offset;
    int64_t lu64ArkSizes = 0;

    for( int ii = 0; ii < miNumArks - 1; ++ii )
    {
        if( lu64Offset < lu64ArkSizes + maArks[ ii ].muSize )
        {
            llOffsetInArk = (unsigned int)( lu64Offset - lu64ArkSizes );
            return ii;
        }

        lu64ArkSizes += maArks[ ii ].muSize;
    }
    
    llOffsetInArk = (unsigned int)( lu64Offset - lu64ArkSizes );

    if( llOffsetInArk > (unsigned int)maArks[ miNumArks - 1 ].muSize )
    {
        return -1;
    }

    return miNumArks - 1;
}

void CHeaderData::SortFileList()
{
    auto lCompare = []( const void* a, const void* b )
    {
        const sFileDef* lpA = (const sFileDef*)a;
        const sFileDef* lpB = (const sFileDef*)b;

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
                return 0;
            }
        }

        return 0;
    };

    std::qsort( maFiles, miNumFiles, sizeof( sFileDef ), lCompare );
}

CEncryptedHeader::CEncryptedHeader()
{
}


CEncryptedHeader::~CEncryptedHeader()
{
    delete[] mpArkData;
}


void CEncryptedHeader::Cycle( unsigned char* lpInStream, unsigned int liStreamSize )
{
    unsigned int liStreamSizeWithoutKey = liStreamSize - 4;

    mlCurrentOffset = 0;
    mlKeyPos = 0;
    miInitialKey = CycleKey( *(int*)lpInStream );
    miCurrentKey = miInitialKey;

    unsigned char* lpByte = lpInStream + 4;
    for( unsigned int ii = 0; ii < liStreamSizeWithoutKey; ++ii, ++lpByte )
    {
        *lpByte = *lpByte ^ ( miCurrentKey ^ 0xFF );
        ++mlCurrentOffset;
        UpdateKey();
    }
}

unsigned int CEncryptedHeader::Load( unsigned char* lpInStream, unsigned int liStreamSize )
{
    Cycle( lpInStream, liStreamSize );

    const unsigned char* lpStream = lpInStream + sizeof( int );
    unsigned int luVersion = ReadFromStream< unsigned int >( lpStream );
    const unsigned int kuUnencryptedVersion = 9;
    if( luVersion != kuUnencryptedVersion )
    {
        std::cout << "ERROR: Unknown version " << luVersion << "\n";
        return luVersion;
    }

    lpStream += 20;     // Skip unknown data

    int liNumArks = ReadFromStream< int >( lpStream );
    mData.SetNumArks( liNumArks );

    int liNumArksSizes = ReadFromStream< int >( lpStream );
    if( liNumArks != liNumArksSizes )
    {
        std::cout << "ERROR: Malformed ark count in header\n";
        return liNumArks - liNumArksSizes;
    }

    int liTotalArkSizes = 0;
    for( int ii = 0; ii < liNumArks; ++ii )
    {
        int liArkSize = ReadFromStream< int >( lpStream );
        mData.SetArkSize( ii, liArkSize );
        liTotalArkSizes += liArkSize;
    }

    int liNumArkPaths = ReadFromStream< int >( lpStream );
    if( liNumArks != liNumArksSizes )
    {
        std::cout << "ERROR: Malformed ark count in header\n";
        return liNumArks - liNumArksSizes;
    }

    std::cout << "Identified " << liNumArks << " ARK files\n";

    for( int ii = 0; ii < liNumArks; ++ii )
    {
        ReadStringToScratch( lpStream );
        mData.SetArkPath( ii, gpScratch );
    }

    int liNumChecksums = ReadFromStream< int >( lpStream );
    if( liNumArks != liNumChecksums )
    {
        std::cout << "ERROR: Malformed ark count in header\n";
        return liNumArks - liNumChecksums;
    }
    for( int ii = 0; ii < liNumChecksums; ++ii )
    {
        int liHash = ReadFromStream< int >( lpStream );
        mData.SetArkHash( ii, liHash );
    }

    int liNumArksWithStrings = ReadFromStream< int >( lpStream );
    if( liNumArks != liNumArksWithStrings )
    {
        std::cout << "ERROR: Malformed ark count in header\n";
        return liNumArks - liNumArksWithStrings;
    }
    for( int ii = 0; ii < liNumArksWithStrings; ++ii )
    {
        int liNumStrings = ReadFromStream< int >( lpStream );
        while( liNumStrings-- > 0 )
        {
            ReadStringToScratch( lpStream );
            mData.AddArkString( ii, gpScratch );
        }
    }

    std::vector< int > laNumInArk;
    laNumInArk.resize( 10 );

    int liNumFiles = ReadFromStream< int >( lpStream );
    mData.SetNumFiles( liNumFiles );
    for( int ii = 0; ii < liNumFiles; ++ii )
    {
        int64_t liOffsetInArk = ReadFromStream< int64_t >( lpStream );
        mData.SetFileOffset( ii, liOffsetInArk );

        ReadStringToScratch( lpStream );
        mData.SetFileName( ii, gpScratch );
        
        int liFlags = ReadFromStream< int >( lpStream );
        mData.SetFileFlags1( ii, liFlags );

        unsigned int liFileSize = ReadFromStream< unsigned int >( lpStream );
        mData.SetFileSize( ii, liFileSize );

        unsigned int liHash = ReadFromStream< unsigned int >( lpStream );
        mData.SetFileHash( ii, liHash );

        unsigned int luOffset = 0;
        int liArk = mData.GetFileArkIndex( ii, luOffset );
        laNumInArk[ liArk ]++;
    }

    int liTotal = 0;
    for( int ii = 0; ii < 10; ++ii )
    {
        std::cout << laNumInArk[ ii ] << " in ark " << ii << "\n";
        liTotal += laNumInArk[ ii ];
    }

    std::cout << liTotal << " in arks\n";

    int liNumFileFlags = ReadFromStream< int >( lpStream );
    if( liNumFileFlags != liNumFiles )
    {
        std::cout << "ERROR: Malformed file count in header\n";
        return liNumFiles - liNumFileFlags;
    }

    for( int ii = 0; ii < liNumFileFlags; ++ii )
    {
        int liFlags = ReadFromStream< int >( lpStream );
        mData.SetFileFlags2( ii, liFlags );
    }
    
    return liStreamSize;
}

bool CEncryptedHeader::Save( int liInitialKey, int liInitialKeyEncoded, bool lbEncrypt )
{
    bool lbSuccess = true;

    mlCurrentOffset = 0;
    miCurrentKey = liInitialKey;
    miInitialKey = liInitialKey;
    mlKeyPos = 0;

    const int kiOutputSizeMax = 1024 * 1024 * 2;
    unsigned char* lpOutputBuffer = new unsigned char[ kiOutputSizeMax ];
    memset( lpOutputBuffer, -1, kiOutputSizeMax );
    *(int*)lpOutputBuffer = liInitialKeyEncoded;

    unsigned char* lpStream = lpOutputBuffer + sizeof( int );
    {
        const unsigned int kuUnencryptedVersion = 9;
        WriteToStream( lpStream, (int)kuUnencryptedVersion );

        memset( lpStream, 0, CHeaderData::kiChecksumSize );  // Skip unknown data
        lpStream += CHeaderData::kiChecksumSize;

        WriteToStream( lpStream, mData.GetNumArks() );
        WriteToStream( lpStream, mData.GetNumArks() );  // Num Ark Sizes
        for( int ii = 0; ii < mData.GetNumArks(); ++ii )
        {
            WriteToStream( lpStream, mData.GetArk( ii )->muSize );
        }

        WriteToStream( lpStream, mData.GetNumArks() );  // Num Ark Paths
        for( int ii = 0; ii < mData.GetNumArks(); ++ii )
        {
            WriteToStream( lpStream, mData.GetArk( ii )->mPath );
        }

        WriteToStream( lpStream, mData.GetNumArks() );  // Num Hashes
        for( int ii = 0; ii < mData.GetNumArks(); ++ii )
        {
            WriteToStream( lpStream, mData.GetArk( ii )->miHash );
        }

        WriteToStream( lpStream, mData.GetNumArks() );  // Num string sets
        for( int ii = 0; ii < mData.GetNumArks(); ++ii )
        {
            const CHeaderData::sArkDef* lpArkDef = mData.GetArk( ii );
            WriteToStream( lpStream, (int)lpArkDef->mStrings.size() );
            for( std::list< std::string >::const_iterator lIter = lpArkDef->mStrings.begin(); lIter != lpArkDef->mStrings.end(); ++lIter )
            {
                WriteToStream( lpStream, *lIter );
            }
        }

        WriteToStream( lpStream, mData.GetNumFiles() );
        for( int ii = 0; ii < mData.GetNumFiles(); ++ii )
        {
            const CHeaderData::sFileDef* lpFileDef = mData.GetFile( ii );
            WriteToStream( lpStream, lpFileDef->mu64Offset );
            WriteToStream( lpStream, lpFileDef->mName );
            WriteToStream( lpStream, lpFileDef->miFlags1 );
            WriteToStream( lpStream, lpFileDef->miSize );
            WriteToStream( lpStream, lpFileDef->miHash );
        }

        WriteToStream( lpStream, mData.GetNumFiles() );     // Num file flags
        for( int ii = 0; ii < mData.GetNumFiles(); ++ii )
        {
            WriteToStream( lpStream, mData.GetFile( ii )->miFlags2 );
        }
    }

    unsigned int liOutputBufferSize = (unsigned int)( lpStream - lpOutputBuffer );
    if( lbEncrypt )
    {
        Cycle( lpOutputBuffer, liOutputBufferSize );
    }

    _mkdir( "packed" );

    FILE* lHeaderFile;
    fopen_s( &lHeaderFile, "packed/main_ps3.hdr", "wb" );

    if( lHeaderFile )
    {
        fwrite( lpOutputBuffer, liOutputBufferSize, 1, lHeaderFile );
        fclose( lHeaderFile );
    }
    else
    {
        lbSuccess = false;
    }

    delete[] lpOutputBuffer;

    if( mpArkData )
    {
        std::cout << "Writing ARK";

        int liArkDataSizeSoFar = 0;
        for( int ii = 0; ii < mData.GetNumArks(); ++ii )
        {
            const CHeaderData::sArkDef* lpArkDef = mData.GetArk( ii );

            std::cout << "...";

            std::string lOutputPath = "packed/";
            lOutputPath.append( lpArkDef->mPath );

            FILE* lArkFile;
            fopen_s( &lArkFile, lOutputPath.c_str(), "wb" );
            if( !lArkFile )
            {
                liArkDataSizeSoFar += lpArkDef->muSize;
                std::cout << "\nFailed to open " << lpArkDef->mPath << " for writing\n";
                continue;
            }

            fwrite( mpArkData + liArkDataSizeSoFar, lpArkDef->muSize, 1, lArkFile );
            fclose( lArkFile );

            std::cout << ii;

            liArkDataSizeSoFar += lpArkDef->muSize;
        }
    }

    std::cout << "\nDone\n";

    return lbSuccess;
}

unsigned char* CEncryptedHeader::Save( unsigned char* lpStream, unsigned int liSize, int liInitialKey, int liInitialKeyEncoded )
{
    Cycle( lpStream, liSize );

    FILE* lHeaderFile;
    fopen_s( &lHeaderFile, "packed/main_ps3.hdr", "wb" );

    if( lHeaderFile )
    {
        fwrite( lpStream, liSize, 1, lHeaderFile );
        fclose( lHeaderFile );
    }

    return lpStream;
}

void CEncryptedHeader::ExtractFiles( const char* lpOutPath )
{
    for( int ii = 0; ii < mData.GetNumFiles(); ++ii )
    {
        ExtractFile( ii, lpOutPath );
    }
}

void CEncryptedHeader::ExtractFile( int liIndex, const char* lpOutPath )
{
    const CHeaderData::sFileDef* lpFileDef = mData.GetFile( liIndex );

    std::string lOutPath = lpOutPath;
    if( lpOutPath[ strlen( lpOutPath ) - 1 ] != '/' )
    {
        lOutPath.append( "/" );
    }
    _mkdir( lOutPath.c_str() );
    std::string lFilename = lOutPath;
    lFilename.append( lpFileDef->mName.c_str() );

    std::cout << "Extracting " << lpFileDef->mName.c_str() << "\n";
    if( lpFileDef->miSize == 0 )
    {
        FILE* lDataFile;
        fopen_s( &lDataFile, lFilename.c_str(), "wb" );
        fclose( lDataFile );
        return;
    }

    unsigned char* lpFileData = new unsigned char[ lpFileDef->miSize ];

    unsigned int luOffset = 0;
    int liArkIndex = mData.GetFileArkIndex( liIndex, luOffset );

    const CHeaderData::sArkDef* lpArkDef = mData.GetArk( liArkIndex );

    FILE* lArkFile;
    fopen_s( &lArkFile, lpArkDef->mPath.c_str(), "rb" );
    fseek( lArkFile, luOffset, SEEK_SET );
    fread( lpFileData, lpFileDef->miSize, 1, lArkFile );
    fclose( lArkFile );

    sprintf_s( gpScratch, "%s", lpFileDef->mName.c_str() );

    size_t liFilenameLength = strlen( gpScratch );
    char* lpNextScratch = gpScratch + liFilenameLength + 1;
    memcpy( lpNextScratch, gpScratch, liFilenameLength );
    lpNextScratch[ liFilenameLength ] = 0;

    const char* lpDirectoryDelim = strstr( gpScratch, "/" );
    while( lpDirectoryDelim )
    {
        lpNextScratch[ lpDirectoryDelim - gpScratch ] = 0;

        std::string lPathName = lOutPath;
        lPathName.append( lpNextScratch );
        _mkdir( lPathName.c_str() );

        struct stat info;
        if( stat( lPathName.c_str(), &info ) != 0 ||
            ( info.st_mode & S_IFDIR ) == 0 )
        {
            std::cout << "Error creating output directory named '" << lPathName.c_str() << "'\n";
            break;
        }

        lpNextScratch[ lpDirectoryDelim - gpScratch ] = '/';
        lpDirectoryDelim = strstr( lpDirectoryDelim + 1, "/" );
    }

    FILE* lDataFile;
    fopen_s( &lDataFile, lFilename.c_str(), "wb" );
    fwrite( lpFileData, lpFileDef->miSize, 1, lDataFile );
    fclose( lDataFile );

    delete[] lpFileData;
}

void CEncryptedHeader::Construct( const char* lpInPath )
{
    mData.SetNumArks( mpReferenceHeader->mData.GetNumArks() );
    for( int ii = 0; ii < mData.GetNumArks(); ++ii )
    {
        const CHeaderData::sArkDef* lpArkDef = mpReferenceHeader->mData.GetArk( ii );
        mData.SetArkDef( ii, *lpArkDef );
    }

    std::string lInPath = lpInPath;
    if( lpInPath[ strlen( lpInPath ) - 1 ] != '/' )
    {
        lInPath.append( "/" );
    }
    std::cout << "Scanning " << lInPath.c_str() << " for files...";

    char* lpScratch = gpScratch;
    unsigned int luDataSize = 0;
    int liNumFiles = GenerateFileList( lInPath.c_str(), lpScratch, &luDataSize );
    int liNumDuplicateFiles = mpReferenceHeader->mData.GetNumDuplicates( gpScratch, liNumFiles );

    mpArkData = new unsigned char[ luDataSize ];
    unsigned char* lpArkPtr = mpArkData;

    mData.SetNumFiles( liNumFiles + liNumDuplicateFiles );
    const char* lpNextPath = gpScratch;

    int liFileOffset = 0;
    for( int ii = 0; ii < liNumFiles; ++ii )
    {
        int jj = 0;
        const CHeaderData::sFileDef* lpFileDef = mpReferenceHeader->mData.GetFile( lpNextPath, jj++ );
        lpNextPath += strlen( lpNextPath ) + 1;

        if( !lpFileDef )
        {
            mData.ReduceFileCount();
            --liFileOffset;
            continue;
        }

        do
        {
            mData.SetFileName( ii + liFileOffset, lpFileDef->mName.c_str() );
            mData.SetFileOffset( ii + liFileOffset, lpFileDef->mu64Offset );
            mData.SetFileFlags1( ii + liFileOffset, lpFileDef->miFlags1 );
            mData.SetFileFlags2( ii + liFileOffset, lpFileDef->miFlags2 );
            mData.SetFileHash( ii + liFileOffset, lpFileDef->miHash );
            mData.SetFileSize( ii + liFileOffset, lpFileDef->miSize );
            
            lpFileDef = mpReferenceHeader->mData.GetFile( lpFileDef->mName.c_str(), jj++ );
            if( lpFileDef )
            {
                ++liFileOffset;
            }
        } while( lpFileDef );
    }

    mData.SortFileList();

    std::vector< unsigned int > laArkSizes;
    unsigned char* lpPreviousArkPtr = mpArkData;

    bool lbCustomOrdering = true;
    if( !lbCustomOrdering )
    {
        laArkSizes.push_back( 156857527 );
        laArkSizes.push_back( 8323577 );
        laArkSizes.push_back( 2832838 );
        laArkSizes.push_back( 56878229 );
        laArkSizes.push_back( 53918014 );
        laArkSizes.push_back( 42595412 );
        laArkSizes.push_back( 0 );
        laArkSizes.push_back( 504516884 );
        laArkSizes.push_back( 491686236 );
        laArkSizes.push_back( 253514521 );
    }

    const unsigned int kuMaxArkSize = (unsigned int)1024 * (unsigned int)1024 * (unsigned int)2048;
    for( int ii = 0; ii < mData.GetNumFiles(); ++ii )
    {
        const CHeaderData::sFileDef* lpFileDef = mData.GetFile( ii );
         
        std::string lFilePath = lInPath;
        lFilePath.append( lpFileDef->mName );

        FILE* lFile = nullptr;
        fopen_s( &lFile, lFilePath.c_str(), "rb" );
        if( lFile )
        {
            fseek( lFile, 0, SEEK_END );
            int liFileSize = ftell( lFile );
            fseek( lFile, 0, SEEK_SET );

            if( ( lpArkPtr - lpPreviousArkPtr ) + liFileSize > kuMaxArkSize )
            {
                unsigned int liThisArkSize = (unsigned int)( lpArkPtr - lpPreviousArkPtr );
                laArkSizes.push_back( liThisArkSize );
                lpPreviousArkPtr = lpArkPtr;
            }

            if( !lbCustomOrdering )
            {
                fread( &mpArkData[ lpFileDef->mu64Offset ], liFileSize, 1, lFile );
            }
            else
            {
                fread( lpArkPtr, liFileSize, 1, lFile );
                mData.SetFileOffset( ii, lpArkPtr - mpArkData );
                lpArkPtr += liFileSize;
            }

            fclose( lFile );
            mData.SetFileSize( ii, liFileSize );
        }
        else
        {
            std::cout << "ERROR - Failed to open file " << lpFileDef->mName.c_str() << "\n";
        }
    }

    if( lbCustomOrdering )
    {
        unsigned int liFinalArkSize = (unsigned int)( lpArkPtr - lpPreviousArkPtr );
        laArkSizes.push_back( liFinalArkSize );
    }
    
    mData.SetNumArks( (int)laArkSizes.size() );
    for( int ii = 0; ii < laArkSizes.size(); ++ii )
    {
        const CHeaderData::sArkDef* lpArkDef = mpReferenceHeader->mData.GetArk( ii );
        mData.SetArkHash( ii, lpArkDef->miHash );

        mData.SetArkSize( ii, (unsigned int)laArkSizes[ ii ] );

        char laValueString[ 4 ];
        _itoa_s( ii, laValueString, 10 );

        std::string lPath = "main_ps3_";
        lPath.append( laValueString );
        lPath.append( ".ark" );
        mData.SetArkPath( ii, lPath.c_str() );
    }

    std::cout << " found " << liNumFiles << " files\n";
}

int CEncryptedHeader::CycleKey( int liKey ) const
{
    int liResult = ( liKey - ( ( liKey / 0x1F31D ) * 0x1F31D ) ) * 0x41A7 - ( liKey / 0x1F31D ) * 0xB14;
    if( liResult <= 0 )
    {
        liResult += 0x7FFFFFFF;
    }
    
    return liResult;
}


void CEncryptedHeader::UpdateKey()
{
    if( mlKeyPos == mlCurrentOffset )
    {
        return;
    }

    if( mlKeyPos > mlCurrentOffset )
    {
        mlKeyPos = 0;
        miCurrentKey = miInitialKey;
    }
    while( mlKeyPos < mlCurrentOffset )
    {
        miCurrentKey = CycleKey( miCurrentKey );
        mlKeyPos++;
    }
}

int CEncryptedHeader::GenerateFileList( const char* lpDirectory, char*& lpScratchPtr, unsigned int* lpOutTotalFileSize )
{
    int liNumFiles = 0;

    std::string lSearchString = lpDirectory;
    lSearchString.append( "*.*" );

    WIN32_FIND_DATAA lFindFileData;
    HANDLE lFindFileHandle = FindFirstFileA( lSearchString.c_str(), &lFindFileData );
    do
    {
        if( ( lFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
        {
            std::string lFilename = ( strstr( lpDirectory, "/" ) + 1 );
            lFilename.append( lFindFileData.cFileName );

            unsigned int liFilenameLength = (unsigned int)strlen( lFilename.c_str() );
            memcpy( lpScratchPtr, lFilename.c_str(), liFilenameLength );
            lpScratchPtr += liFilenameLength;
            *lpScratchPtr = 0;
            ++lpScratchPtr;

            ++liNumFiles;

            std::string lSourceFilename = "out/";
            lSourceFilename += lFilename.c_str();

            FILE* lFile = nullptr;
            fopen_s( &lFile, lSourceFilename.c_str(), "rb" );
            if( !lFile )
            {
                std::cout << "Unable to open file: " << lSourceFilename.c_str() << "\n";
                continue;
            }

            fseek( lFile, 0, SEEK_END );
            int liFileSize = ftell( lFile );
            fclose( lFile );

            if( lpOutTotalFileSize )
            {
                *lpOutTotalFileSize += liFileSize;
            }
        }
    } while( FindNextFileA( lFindFileHandle, &lFindFileData ) != 0 );

    lFindFileHandle = FindFirstFileA( lSearchString.c_str(), &lFindFileData );
    do
    {
        if( lFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
            if( lFindFileData.cFileName[ 0 ] == '.' &&
                ( lFindFileData.cFileName[ 1 ] == '.' || lFindFileData.cFileName[ 1 ] == 0 ) )
            {
                continue;
            }

            lSearchString = lpDirectory;
            lSearchString.append( lFindFileData.cFileName );
            lSearchString.append( "/" );

            liNumFiles += GenerateFileList( lSearchString.c_str(), lpScratchPtr, lpOutTotalFileSize );
        }
    } while( FindNextFileA( lFindFileHandle, &lFindFileData ) != 0 );

    *lpScratchPtr = 0;

    return liNumFiles;
}
