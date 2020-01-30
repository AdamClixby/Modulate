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

int main( int argc, char *argv[], char *envp[] )
{
    if( argc != 3 )
    {
        std::cout << "Requires two args\n";
        return -1;
    }

    const unsigned int kuUnencryptedVersion = 9;
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

    for( int argi = 2; argi < argc; ++argi )
    {
        if( !_stricmp( argv[ argc ], "moggdetails" ) )
        {

        }
        else if( _stricmp( argv[ argi ], "loadsave" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.Save( 1739967672, -967906000 );
        }
        else if( _stricmp( argv[ argi ], "decrypt" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.Save( 1739967672, -967906000, false );
        }
        else if( _stricmp( argv[ argi ], "unpack" ) == 0 ||
                 _stricmp( argv[ argi ], "extract" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.ExtractFiles();
        }
        else if( _stricmp( argv[ argi ], "pack" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );

            CEncryptedHeader lModifiedHeader;
            lModifiedHeader.SetReference( &lHeader );

            lModifiedHeader.Construct( "out" );
            lModifiedHeader.Save( 1739967672, -967906000, true );
        }
    }

    return 0;
}
