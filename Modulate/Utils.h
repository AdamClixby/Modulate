#pragma once

#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>

static char gpScratch[ 1024 * 1024 ];

template <typename T>
static T ReadFromStream( const unsigned char*& lpStream )
{
    T lpResult = *(T*)lpStream;
    lpStream += sizeof( T );
    return lpResult;
}

template <typename T>
static void WriteToStream( unsigned char*& lpStream, const T& lValue )
{
    *(T*)lpStream = lValue;
    lpStream += sizeof( T );
}

static int WriteToStream( unsigned char*& lpStream, const std::string& lString )
{
    int liLength = (int)lString.length();
    WriteToStream( lpStream, liLength );
    memcpy( lpStream, lString.c_str(), liLength );
    lpStream += liLength;

    return liLength;
}

static void ReadStringToScratch( const unsigned char*& lpStream )
{
    int liFilenameLength = ReadFromStream< int >( lpStream );
    memcpy( gpScratch, lpStream, liFilenameLength );
    gpScratch[ liFilenameLength ] = 0;
    lpStream += liFilenameLength;
}

static int IntToCSV( const unsigned char*& lpStream, unsigned char*& lpCSV )
{
    int liVal = ReadFromStream< int >( (const unsigned char*&)lpStream );
    std::cout << liVal << "\n";

    _itoa_s( liVal, gpScratch, 10 );
    size_t liStringLength = strlen( gpScratch );
    memcpy( lpCSV, gpScratch, liStringLength );
    lpCSV += liStringLength;
    *lpCSV++ = '\t';

    return liVal;
};

static short ShortToCSV( const unsigned char*& lpStream, unsigned char*& lpCSV )
{
    short liVal = ReadFromStream< short >( (const unsigned char*&)lpStream );
    std::cout << liVal << "\n";

    _itoa_s( liVal, gpScratch, 10 );
    size_t liStringLength = strlen( gpScratch );
    memcpy( lpCSV, gpScratch, liStringLength );
    lpCSV += liStringLength;
    *lpCSV++ = '\t';

    return liVal;
};

static const char* StringToCSV( const unsigned char*& lpStream, unsigned char*& lpCSV )
{
    ReadStringToScratch( (const unsigned char*&)lpStream );
    std::cout << gpScratch << "\n";

    size_t liStringLength = strlen( gpScratch );
    memcpy( lpCSV, gpScratch, liStringLength );
    lpCSV += liStringLength;
    *lpCSV++ = '\t';

    return gpScratch;
};

static void IntFromCSV( char*& lpCSV, char* lpOutput )
{
    char* lpTab = strstr( lpCSV, "\t" );
    *lpTab = 0;

    std::cout << lpCSV << "\n";

    int liVal = atoi( lpCSV );
    *(int*)lpOutput = liVal;
    lpOutput += sizeof( int );
    lpCSV = lpTab + 1;
}

static void ShortFromCSV( char*& lpCSV, char* lpOutput )
{
    char* lpTab = strstr( lpCSV, "\t" );
    *lpTab = 0;

    std::cout << lpCSV << "\n";

    short liVal = atoi( lpCSV );
    *(short*)lpOutput = liVal;
    lpOutput += sizeof( short );
    lpCSV = lpTab + 1;
};

static void StringFromCSV( char*& lpCSV, char* lpOutput )
{
    char* lpTab = strstr( lpCSV, "\t" );
    *lpTab = 0;

    int liLength = (int)( lpTab - lpCSV );
    *(int*)lpOutput = liLength;
    lpOutput += sizeof( int );

    std::cout << lpCSV << "\n";

    memcpy( lpOutput, lpCSV, liLength );
    lpOutput += liLength;
    lpCSV = lpTab + 1;
};
