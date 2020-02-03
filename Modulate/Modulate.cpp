// Modulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>

#include "Utils.h"
#include "CEncryptedHeader.h"
#include "CAmpConfig.h"

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

void ExtractConfig( const char* lpFilePath, const char* lpOutputFile )
{
    CAmpConfig lConfig;
    lConfig.Load( lpFilePath );
}

void ExtractStrings( const char* lpFilePath, const char* lpOutputFile )
{
    std::string lFilePath = lpFilePath;
    if( lpFilePath[ strlen( lpFilePath ) - 1 ] != '/' )
    {
        lFilePath.append( "/" );
    }
    lFilePath.append( "locale_keep.dta_dta_ps3" );

    FILE* lLocaleFile = nullptr;
    fopen_s( &lLocaleFile, lFilePath.c_str(), "rb" );
    if( !lLocaleFile )
    {
        std::cout << "Failed to open locale file " << lpFilePath << "\n";
        return;
    }
    fseek( lLocaleFile, 0, SEEK_END );
    
    std::cout << "Extracting strings from " << lFilePath.c_str() << "\n\n";

    int liFileSize = ftell( lLocaleFile );
    unsigned char* lpFileContents = new unsigned char[ liFileSize ];
    unsigned char* lpCSVData = new unsigned char[ liFileSize ];

    fseek( lLocaleFile, 0, SEEK_SET );
    fread( lpFileContents, liFileSize, 1, lLocaleFile );
    fclose( lLocaleFile );

    unsigned char* lpCSVPtr = lpCSVData;

    const unsigned char* lpStream = lpFileContents;
    lpStream += 9;

    int liNumStrings = 0;
    do
    {
        IntToCSV( lpStream, lpCSVPtr );
        IntToCSV( lpStream, lpCSVPtr );
        ShortToCSV( lpStream, lpCSVPtr );
        ShortToCSV( lpStream, lpCSVPtr );
        IntToCSV( lpStream, lpCSVPtr );
        
        StringToCSV( lpStream, lpCSVPtr );
        std::cout << gpScratch << " :: ";

        IntToCSV( lpStream, lpCSVPtr );
        
        StringToCSV( lpStream, lpCSVPtr );
        std::cout << gpScratch << "\n";

        ++liNumStrings;
    } while( lpStream - lpFileContents < liFileSize);

    delete[] lpFileContents;

    FILE* lCSVFile = nullptr;
    fopen_s( &lCSVFile, lpOutputFile, "wb" );
    if( lCSVFile )
    {
        fwrite( lpCSVData, lpCSVPtr - lpCSVData, 1, lCSVFile );
        fclose( lCSVFile );
    }
    else
    {
        std::cout << "Failed to create file for writing - " << lpOutputFile << "\n";
    }

    delete[] lpCSVData;

    std::cout << "\nWrote " << liNumStrings << " strings to " << lpOutputFile << "\n";
}

void EncodeStrings( const char* lpStringsFile, const char* lpOutputFilename )
{
    FILE* lStringsFile = nullptr;
    fopen_s( &lStringsFile, lpStringsFile, "rb" );
    if( !lStringsFile )
    {
        std::cout << "Failed to open strings file " << lpStringsFile << "\n";
        return;
    }

    fseek( lStringsFile, 0, SEEK_END );

    int liFileSize = ftell( lStringsFile );
    char* lpCSVData = new char[ liFileSize ];
    char* lpOutputData = new char[ liFileSize * 2 ];

    fseek( lStringsFile, 0, SEEK_SET );
    fread( lpCSVData, liFileSize, 1, lStringsFile );
    fclose( lStringsFile );

    std::cout << "Encoding strings from " << lpStringsFile << "\n\n";

    char* lpOutputPtr = lpOutputData;
    char* lpCSVPtr = lpCSVData;
   
    // Header
    *lpOutputPtr++ = 1;
    *(int*)lpOutputPtr = 1;
    lpOutputPtr += sizeof( int );

    int* lpStringCount = (int*)lpOutputPtr;
    lpOutputPtr += sizeof( int );

    int liNumStrings = 0;
    while( lpCSVPtr - lpCSVData < liFileSize )
    {
        IntFromCSV( lpCSVPtr, lpOutputPtr );
        IntFromCSV( lpCSVPtr, lpOutputPtr );
        ShortFromCSV( lpCSVPtr, lpOutputPtr );
        ShortFromCSV( lpCSVPtr, lpOutputPtr );
        IntFromCSV( lpCSVPtr, lpOutputPtr );

        const char* lpKey = lpCSVPtr;
        StringFromCSV( lpCSVPtr, lpOutputPtr );
        std::cout << lpKey << " :: ";
        
        IntFromCSV( lpCSVPtr, lpOutputPtr );

        const char* lpTranslation = lpCSVPtr;
        StringFromCSV( lpCSVPtr, lpOutputPtr );
        std::cout << lpTranslation << "\n";

        ++liNumStrings;
    }

    *lpStringCount = liNumStrings;

    FILE* lOutputFile = nullptr;
    fopen_s( &lOutputFile, lpOutputFilename, "wb" );
    if( lOutputFile )
    {
        fwrite( lpOutputData, ( lpOutputPtr - lpOutputData ), 1, lOutputFile );
        fclose( lOutputFile );
    }
    else
    {
        std::cout << "Failed to open " << lpOutputFilename << " for writing\n";
    }
}

void CompileMoggs( const char* lpOutDir )
{
    std::string lOutDir = lpOutDir;
    if( lpOutDir[ strlen( lpOutDir ) - 1 ] != '/' )
    {
        lOutDir.append( "/" );
    }

    char* lpScratch = gpScratch;
    CEncryptedHeader::GenerateFileList( lOutDir.c_str(), lpScratch, nullptr );

    const char* kpMoggSongExtension = ".moggsong";
    const char* lpFilename = gpScratch;
    while( *lpFilename )
    {
        if( strstr( lpFilename, kpMoggSongExtension ) )
        {
            std::cout << lpFilename << "\n";
        }

        lpFilename += strlen( lpFilename ) + 1;
    }
}

int main( int argc, char *argv[], char *envp[] )
{
    if( argc < 3 )
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
        if( _stricmp( argv[ argi ], "extractconfig" ) == 0 )
        {
            ExtractConfig( "out/ps3/config/amplitude.dta_dta_ps3", "config.txt" );
        }
        else if( _stricmp( argv[ argi ], "extractstrings" ) == 0 )
        {
            ExtractStrings( "out/ps3/ui/locale/eng", "strings.txt" );
        }
        else if( _stricmp( argv[ argi ], "encodestrings" ) == 0 )
        {
            EncodeStrings( "strings.txt", "out/ps3/ui/locale/eng/locale_keep.dta_dta_ps3" );
        }
        else if( _stricmp( argv[ argi ], "compilemoggs" ) == 0 )
        {
            CompileMoggs( "out" );
        }
        else if( _stricmp( argv[ argi ], "loadsave" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.Save( 1739967672, -967906000 );
            lHeader.Cycle( lpHeaderData, liHeaderSize );
        }
        else if( _stricmp( argv[ argi ], "decrypt" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.Save( 1739967672, -967906000, false );
            lHeader.Cycle( lpHeaderData, liHeaderSize );
        }
        else if( _stricmp( argv[ argi ], "unpack" ) == 0 ||
                 _stricmp( argv[ argi ], "extract" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.ExtractFiles( "out" );
            lHeader.Cycle( lpHeaderData, liHeaderSize );
        }
        else if( _stricmp( argv[ argi ], "pack" ) == 0 )
        {
            CEncryptedHeader lHeader;
            lHeader.Load( lpHeaderData, liHeaderSize );
            lHeader.Cycle( lpHeaderData, liHeaderSize );

            CEncryptedHeader lModifiedHeader;
            lModifiedHeader.SetReference( &lHeader );

            lModifiedHeader.Construct( "out" );
            lModifiedHeader.Save( 1739967672, -967906000, true );
        }
    }

    delete[] lpHeaderData;

    return 0;
}
