#include "pch.h"
#include "CAmpConfig.h"
#include "Utils.h"

#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>

CAmpConfig::CAmpConfig()
{
}


CAmpConfig::~CAmpConfig()
{
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
    lpStream += 12181;

    while( lpStream )
    {
        std::cout << "\n";

        int liVal1 = IntToCSV( lpStream, lpCSVPtr );                // 1
        short liVal2 = ShortToCSV( lpStream, lpCSVPtr );            // 6
        short liVal3 = ShortToCSV( lpStream, lpCSVPtr );
        int liVal4 = IntToCSV( lpStream, lpCSVPtr );                // 5

        if( liVal4 == 0 )
        {
            while( ReadFromStream< int >( lpStream ) != 15 ) {}
            continue;
        }
        const char* lpString1 = StringToCSV( lpStream, lpCSVPtr );  // Name

        if( liVal3 == 10 )
        {
            ReadFromStream< int >( lpStream );
            StringToCSV( lpStream, lpCSVPtr );
        }

        int liVal5 = IntToCSV( lpStream, lpCSVPtr );
        while( liVal5 == 5 ||
            liVal5 == 18 ||
            liVal5 == 35 )
        {
            StringToCSV( lpStream, lpCSVPtr );
            liVal5 = ReadFromStream< int >( lpStream );
        }

        if( liVal5 == 9 )
        {
            IntToCSV( lpStream, lpCSVPtr );
            liVal5 = ReadFromStream< int >( lpStream );
        }

        while( liVal5 == 0 )
        {
            while( liVal5 != 5 )
            {
                liVal5 = IntToCSV( lpStream, lpCSVPtr );
            }

            while( liVal5 == 5 )
            {
                StringToCSV( lpStream, lpCSVPtr );
                liVal5 = ReadFromStream< int >( lpStream );
            }
        }

        if( liVal5 == 1 ||
            liVal5 == 16 ||
            liVal5 == 17 )
        {
            continue;
        }

        IntToCSV( lpStream, lpCSVPtr );     // 5
        StringToCSV( lpStream, lpCSVPtr );  // String table entry for name
        IntToCSV( lpStream, lpCSVPtr );     // 5
        StringToCSV( lpStream, lpCSVPtr );  // Unlock type
        IntToCSV( lpStream, lpCSVPtr );     // 5
        StringToCSV( lpStream, lpCSVPtr );  // Unlock desc
        IntToCSV( lpStream, lpCSVPtr );     // 18
        StringToCSV( lpStream, lpCSVPtr );  // Image
        IntToCSV( lpStream, lpCSVPtr );     // 5
        StringToCSV( lpStream, lpCSVPtr );  // VO
    }

    delete[] lpFileContents;
}
