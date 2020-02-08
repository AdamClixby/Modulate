#include "pch.h"
#include "CAmpConfig.h"
#include "Utils.h"

#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>

const int kiInt = 0;
const int kiType = 1;
const int kiString = 5;
const int kiEntryComplete = 16;
const int kiInputBinding = 17;
const int kiPath = 18;
const int kiFileRef = 33;

CAmpConfig::CAmpConfig()
{
}


CAmpConfig::~CAmpConfig()
{
}

void CAmpConfig::ReadUnlockDescription( const unsigned char*& lpStream, int liMaxToRead )
{
    const unsigned char* lpInitialStream = lpStream;
    do {
        int liVal = ReadFromStream< int >( lpStream );
        switch( liVal )
        {
        case kiInt:
        case 9:
            std::cout << "Int " << ReadFromStream< int >( lpStream ) << "\t";
            break;

        case 1:
//            std::cout << "Int " << ReadFromStream< int >( lpStream ) << "\n";
            std::cout << "Byte " << (int)ReadFromStream< unsigned char >( lpStream ) << "\t";
            std::cout << "Byte " << (int)ReadFromStream< unsigned char >( lpStream ) << "\t";
            std::cout << "Byte " << (int)ReadFromStream< unsigned char >( lpStream ) << "\t";
            std::cout << "Byte " << (int)ReadFromStream< unsigned char >( lpStream ) << "\t";
            break;

        case kiFileRef:
            std::cout << "File - " << ReadStringToScratch( lpStream ) << "\n";
            break;
        case 35:
            std::cout << "35 - " << ReadStringToScratch( lpStream ) << "\n";
            break;
        case kiString:
            std::cout << "String - " << ReadStringToScratch( lpStream ) << "\n";
            break;
        case kiPath:
            std::cout << "Path - " << ReadStringToScratch( lpStream ) << "\n";
            break;

        case kiEntryComplete:
        case kiInputBinding:
            std::cout << "\n";
            return;

        default:
            break;
        }
    } while( lpStream - lpInitialStream < liMaxToRead );
}

CAmpConfig::sUnlock CAmpConfig::ReadUnlock( const unsigned char*& lpStream, int liMaxToRead )
{
    const unsigned char* lpInitialStream = lpStream;

    sUnlock lUnlock;

    do
    {
        int liVal = ReadFromStream< int >( lpStream );
        switch( liVal )
        {
        case kiType:
        {
            short liType = ReadFromStream< short >( lpStream );
            std::cout << "Type is " << liType << ":\t";

            short liEntryNum = ReadFromStream< short >( lpStream );
            std::cout << "Entry " << liEntryNum << "\n";

            ReadUnlockDescription( lpStream, (int)( liMaxToRead - ( lpStream - lpInitialStream ) ) );
        }

        case kiString:
        case kiPath:
            std::cout << ReadStringToScratch( lpStream ) << "\t";
            break;

        case kiEntryComplete:
            return lUnlock;

        default:
            break;
        }
    } while( lpStream - lpInitialStream < liMaxToRead );

    return lUnlock;
}

void CAmpConfig::Load( const char* lpPath )
{
    FILE* lConfigFile = nullptr;
    fopen_s( &lConfigFile, lpPath, "rb" );
    if( !lConfigFile )
    {
        std::cout << "Failed to open config file " << lpPath << "\n";
        return;
    }
    fseek( lConfigFile, 0, SEEK_END );

    std::cout << "Extracting config from " << lpPath << "\n\n";

    int liFileSize = ftell( lConfigFile );
    unsigned char* lpFileContents = new unsigned char[ liFileSize ];
    unsigned char* lpCSVData = new unsigned char[ liFileSize ];

    fseek( lConfigFile, 0, SEEK_SET );
    fread( lpFileContents, liFileSize, 1, lConfigFile );
    fclose( lConfigFile );

    unsigned char* lpCSVPtr = lpCSVData;
    const unsigned char* lpStream = lpFileContents;
    lpStream++;

    ReadUnlock( lpStream, (int)(liFileSize - ( lpStream - lpFileContents ) ) );
    
    delete[] lpFileContents;
}
