#pragma once

#include <list>
#include <string>

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

class CHeaderData
{
private:
    int miNumArks = 0;
    int miNumFiles = 0;

public:
    static const int kiChecksumSize = 20;

    struct sArkDef
    {
        std::string mPath;
        int miSize = 0;
        int miHash = 0;
        std::list< std::string > mStrings;
    };
private:
    sArkDef* maArks = nullptr;

public:
    struct sFileDef
    {
        std::string mName;
        int64_t mu64Offset = 0;
        int miFlags1 = 0;
        int miFlags2 = 0;
        int miHash = 0;
        int miSize = 0;
    };
private:
    sFileDef* maFiles = nullptr;

public:
    void SetNumArks( int liNum )
    {
        delete[] maArks;
        maArks = new sArkDef[ liNum ];
        miNumArks = liNum;
    }

    void SetArkSize( int liArkNum, int liArkSize )          {   maArks[ liArkNum ].miSize = liArkSize;  }
    void SetArkPath( int liArkNum, const char* lpPath )     {   maArks[ liArkNum ].mPath  = lpPath;     }
    void SetArkHash( int liArkNum, int liHash )             {  /* maArks[ liArkNum ].miHash = liHash;*/     }
    void AddArkString( int liArkNum, const char* lpString ) {   maArks[ liArkNum ].mStrings.push_back( std::string( lpString ) ); }

    void SetNumFiles( int liNum )
    {
        delete[] maFiles;
        maFiles = new sFileDef[ liNum ];
        miNumFiles = liNum;
    }
    void SetFileOffset( int liFileNum, int64_t lu64Offset ) {   maFiles[ liFileNum ].mu64Offset = lu64Offset; }
    void SetFileName( int liFileNum, const char* lpName )   {   maFiles[ liFileNum ].mName      = lpName;     }
    void SetFileFlags1( int liFileNum, int liFlags )        {   maFiles[ liFileNum ].miFlags1   = liFlags;    }
    void SetFileFlags2( int liFileNum, int liFlags )        {   maFiles[ liFileNum ].miFlags2   = liFlags;    }
    void SetFileHash( int liFileNum, int liHash )           {  /* maFiles[ liFileNum ].miHash     = liHash;*/     }
    void SetFileSize( int liFileNum, int liSize )           {   maFiles[ liFileNum ].miSize     = liSize;     }

    int GetFileArkIndex( int liFileNum, long& llOffsetInArk ) const;

    int GetNumArks() const  { return miNumArks; }
    int GetNumFiles() const { return miNumFiles; }
    
    const sArkDef* GetArk( int liArkNum ) const    { return &maArks[ liArkNum ]; }
    const sFileDef* GetFile( int liFileNum ) const { return &maFiles[ liFileNum ]; }
};

class CEncryptedHeader
{
public:
    CEncryptedHeader();
    ~CEncryptedHeader();
    
    void Cycle( unsigned char* lpInStream, unsigned int liStreamSize );

    unsigned int Load( unsigned char* lpStream, unsigned int liStreamSize );
    unsigned char* Save( unsigned char* lpStream, unsigned int liSize, int liInitialKey, int liInitialKeyEncoded );
    bool Save( int liInitialKey, int liInitialKeyEncoded );

    void ExtractFiles();

private:
    void ExtractFile( int liIndex );
    int CycleKey( int liKey ) const;
    void UpdateKey();

    CHeaderData mData;

    int miInitialKey;
    int miCurrentKey;

    long mlCurrentOffset;
    long mlKeyPos;
};

