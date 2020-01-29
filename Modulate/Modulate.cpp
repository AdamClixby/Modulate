// Modulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>

#include "CEncryptedHeader.h"

unsigned char* GetDecryptedHeaderData( const char* lpFilename )
{
    FILE* lHeaderFile = nullptr;
    fopen_s( &lHeaderFile, lpFilename, "rb" );
    if( !lHeaderFile )
    {
        std::cout << "Unable to open header file: " << lpFilename << "\n";
        return nullptr;
    }

    fseek( lHeaderFile, 0, SEEK_END );
    int liHeaderSize = ftell( lHeaderFile );
    unsigned char* lpHeaderData = new unsigned char[ liHeaderSize ];
    unsigned char* lpDecryptedHeaderData = new unsigned char[ liHeaderSize ];

    std::cout << "Reading header data (" << liHeaderSize << " bytes)\n";

    fseek( lHeaderFile, 0, SEEK_SET );
    fread( lpHeaderData, liHeaderSize, 1, lHeaderFile );
    fclose( lHeaderFile );

    memcpy( lpDecryptedHeaderData, lpHeaderData, liHeaderSize );

    unsigned int luVersion = *(unsigned int*)( lpHeaderData );
    const unsigned int kuEncryptedVersion = 0xc64eed30;
    if( luVersion != kuEncryptedVersion )
    {
        std::cout << "ERROR: Unknown version " << luVersion << "\n";
        return nullptr;
    }

    CEncryptedHeader lHeader;
    liHeaderSize = lHeader.Load( lpDecryptedHeaderData, liHeaderSize );

    unsigned char* lpReencrypedHeaderData = lHeader.Save( lpDecryptedHeaderData, liHeaderSize - 4, 1739967672 , -967906000 );

    int liSame = 0;
    int liDiff = 0;
    for( int ii = 0; ii < liHeaderSize; ++ii )
    {
        if( lpReencrypedHeaderData[ ii ] == lpHeaderData[ ii ] )
        {
            liSame++;
        }
        else
        {
            liDiff++;
        }
    }

    delete[] lpHeaderData;
    delete[] lpReencrypedHeaderData;

    return lpDecryptedHeaderData;
}

int GenerateFileList( const char* lpDirectory, char*& lpScratchPtr, unsigned int& lOutTotalFileSize )
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

            lOutTotalFileSize += liFileSize;
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

            liNumFiles += GenerateFileList( lSearchString.c_str(), lpScratchPtr, lOutTotalFileSize );
        }
    } while( FindNextFileA( lFindFileHandle, &lFindFileData ) != 0 );

    return liNumFiles;
}

int main( int argc, char *argv[], char *envp[] )
{
    if( argc != 3 )
    {
        std::cout << "Requires two args\n";
        return -1;
    }

    const unsigned int kuUnencryptedVersion = 9;

    for( int argi = 2; argi < argc; ++argi )
    {
        if( _stricmp( argv[ argi ], "unpack" ) == 0 )
        {
            FILE* lHeaderFile = nullptr;
            fopen_s( &lHeaderFile, argv[ 1 ], "rb" );
            if( !lHeaderFile )
            {
                std::cout << "Unable to open header file: " << argv[ 1 ] << "\n";
                return -1;
            }

            fseek( lHeaderFile, 0, SEEK_END );
            int liHeaderSize = ftell( lHeaderFile );
            unsigned char* lpHeaderData = new unsigned char[ liHeaderSize ];

            std::cout << "Reading header data (" << liHeaderSize << " bytes)\n";

            fseek( lHeaderFile, 0, SEEK_SET );
            fread( lpHeaderData, liHeaderSize, 1, lHeaderFile );
            fclose( lHeaderFile );

            unsigned int luVersion = *(unsigned int*)( lpHeaderData );
            const unsigned int kuEncryptedVersion = 0xc64eed30;
            if( luVersion != kuEncryptedVersion )
            {
                std::cout << "ERROR: Unknown version " << luVersion << "\n";
                return -1;
            }

            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.ExtractFiles();
        }
        else if( _stricmp( argv[ argi ], "newpack" ) == 0 )
        {
            FILE* lHeaderFile = nullptr;
            fopen_s( &lHeaderFile, argv[ 1 ], "rb" );
            if( !lHeaderFile )
            {
                std::cout << "Unable to open header file: " << argv[ 1 ] << "\n";
                return -1;
            }

            fseek( lHeaderFile, 0, SEEK_END );
            int liHeaderSize = ftell( lHeaderFile );
            unsigned char* lpHeaderData = new unsigned char[ liHeaderSize ];

            std::cout << "Reading header data (" << liHeaderSize << " bytes)\n";

            fseek( lHeaderFile, 0, SEEK_SET );
            fread( lpHeaderData, liHeaderSize, 1, lHeaderFile );
            fclose( lHeaderFile );

            unsigned int luVersion = *(unsigned int*)( lpHeaderData );
            const unsigned int kuEncryptedVersion = 0xc64eed30;
            if( luVersion != kuEncryptedVersion )
            {
                std::cout << "ERROR: Unknown version " << luVersion << "\n";
                return -1;
            }

            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.Save( 1739967672, -967906000 );
        }
    }

    return 0;
}
