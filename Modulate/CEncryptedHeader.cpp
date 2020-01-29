#include "pch.h"
#include "CEncryptedHeader.h"

#include <windows.h>
#include <iostream>
#include <sys/types.h>
#include <direct.h>

int CHeaderData::GetFileArkIndex( int liFileNum, long& llOffsetInArk ) const
{
    int64_t lu64Offset = maFiles[ liFileNum ].mu64Offset;
    int64_t lu64ArkSizes = maArks[ 0 ].miSize;

    for( int ii = 0; ii < miNumArks - 1; ++ii )
    {
        if( lu64Offset < lu64ArkSizes )
        {
            llOffsetInArk = (long)( lu64Offset - lu64ArkSizes );
            return ii;
        }

        lu64ArkSizes += maArks[ ii + 1 ].miSize;
    }

    llOffsetInArk = (long)(lu64Offset - lu64ArkSizes);
    return miNumArks - 1;
}

CEncryptedHeader::CEncryptedHeader()
{
}


CEncryptedHeader::~CEncryptedHeader()
{
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

    std::cout << "Identified " << liNumArks << " ARK files:\n";

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
    }

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

bool CEncryptedHeader::Save( int liInitialKey, int liInitialKeyEncoded )
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
            WriteToStream( lpStream, mData.GetArk( ii )->miSize );
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
    Cycle( lpOutputBuffer, liOutputBufferSize );

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

void CEncryptedHeader::ExtractFiles()
{
    for( int ii = 0; ii < mData.GetNumFiles(); ++ii )
    {
        ExtractFile( ii );
    }
}

void CEncryptedHeader::ExtractFile( int liIndex )
{
    const CHeaderData::sFileDef* lpFileDef = mData.GetFile( liIndex );
    std::cout << "Extracting " << lpFileDef->mName.c_str() << "\n";
    if( lpFileDef->miSize == 0 )
    {
        FILE* lDataFile;
        fopen_s( &lDataFile, lpFileDef->mName.c_str(), "wb" );
        fclose( lDataFile );
        return;
    }

    unsigned char* lpFileData = new unsigned char[ lpFileDef->miSize ];

    long llOffset = 0;
    int liArkIndex = mData.GetFileArkIndex( liIndex, llOffset );

    const CHeaderData::sArkDef* lpArkDef = mData.GetArk( liArkIndex );

    FILE* lArkFile;
    fopen_s( &lArkFile, lpArkDef->mPath.c_str(), "rb" );
    fseek( lArkFile, llOffset, SEEK_SET );
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

        std::string lFilename = "out/";
        lFilename.append( lpNextScratch );
        _mkdir( lFilename.c_str() );

        struct stat info;
        if( stat( lFilename.c_str(), &info ) != 0 ||
            ( info.st_mode & S_IFDIR ) == 0 )
        {
            std::cout << "Error creating output directory named '" << lFilename.c_str() << "'\n";
            break;
        }

        lpNextScratch[ lpDirectoryDelim - gpScratch ] = '/';
        lpDirectoryDelim = strstr( lpDirectoryDelim + 1, "/" );
    }

    std::string lFilename = "out/";
    lFilename.append( lpFileDef->mName.c_str() );

    FILE* lDataFile;
    fopen_s( &lDataFile, lFilename.c_str(), "wb" );
    fwrite( lpFileData, lpFileDef->miSize, 1, lDataFile );
    fclose( lDataFile );

    delete[] lpFileData;
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
