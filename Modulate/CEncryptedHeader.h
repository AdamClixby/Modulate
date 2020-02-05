#pragma once

#include <list>
#include <string>
#include <vector>
#include "Utils.h"

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
        unsigned int muSize = 0;
        int miHash = 0;
        std::list< std::string > mStrings;
    };
private:
    sArkDef* maArks = nullptr;

public:
    struct sFileDef
    {
        std::string mName;
        size_t mNameHash = 0;
        size_t mExtensionHash = 0;
        int64_t mu64Offset = 0;
        int miFlags1 = -1;
        int miFlags2 = -1;
        int miHash = 0;
        int miSize = 0;
        std::vector< std::string > mPathBreakdown;
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

    void SetArkDef( int liArkNum, const sArkDef& lDef )     {   maArks[ liArkNum ] = lDef; }
    void SetArkSize( int liArkNum, int liArkSize )          {   maArks[ liArkNum ].muSize = liArkSize;  }
    void SetArkPath( int liArkNum, const char* lpPath )     {   maArks[ liArkNum ].mPath  = lpPath;     }
    void SetArkHash( int liArkNum, int liHash )             {   maArks[ liArkNum ].miHash = liHash;     }
    void AddArkString( int liArkNum, const char* lpString ) {   maArks[ liArkNum ].mStrings.push_back( std::string( lpString ) ); }

    void ReduceFileCount()
    {
        miNumFiles--;
    }
    void SetNumFiles( int liNum )
    {
        delete[] maFiles;
        maFiles = new sFileDef[ liNum ];
        miNumFiles = liNum;
    }
    void SetFileOffset( int liFileNum, int64_t lu64Offset ) {   maFiles[ liFileNum ].mu64Offset = lu64Offset; }
    void SetFileName( int liFileNum, const char* lpName )
    {
        sFileDef& lFileDef = maFiles[ liFileNum ];
        
        lFileDef.mName = lpName;
        std::string lName = lpName;
        lFileDef.mNameHash = std::hash< std::string >{}( lName );
        
        const char* lpExtension = lpName + strlen( lpName );
        while( lpExtension > lpName && *lpExtension != '.' )
        {
            --lpExtension;
        }
        if( lpExtension != lpName )
        {
            lFileDef.mExtensionHash = std::hash< std::string >{}( std::string( lpExtension ) );
        }

        int liNameLength = (int)strlen( lpName );
        char* lpNameCopy = new char[ liNameLength + 1 ];
        memcpy( lpNameCopy, lpName, liNameLength + 1 );

        char* lpFolderBreak = nullptr;
        char* lpFolder = lpNameCopy;
        while( lpFolderBreak = strstr( lpFolder, "/" ) )
        {
            *lpFolderBreak = 0;
            lFileDef.mPathBreakdown.push_back( std::string( lpFolder ) );

            lpFolder = lpFolderBreak + 1;
        }

        lFileDef.mPathBreakdown.push_back( std::string( lpFolder ) );

        delete[] lpNameCopy;
    }
    void SetFileFlags1( int liFileNum, int liFlags )        {   maFiles[ liFileNum ].miFlags1   = liFlags;    }
    void SetFileFlags2( int liFileNum, int liFlags )        {   maFiles[ liFileNum ].miFlags2   = liFlags;    }
    void SetFileHash( int liFileNum, int liHash )           {   maFiles[ liFileNum ].miHash     = 0;          }  //liHash;     }
    void SetFileSize( int liFileNum, int liSize )           {   maFiles[ liFileNum ].miSize     = liSize;     }

    int GetFileArkIndex( int liFileNum, unsigned int& llOffsetInArk ) const;

    int GetNumArks() const  { return miNumArks; }
    int GetNumFiles() const { return miNumFiles; }

    int GetNumDuplicates( const char* lpNameList, int liNumNames ) const
    {
        int liNum = 0;
        int liNumIgnored = 0;
        for( int jj = 0; jj < liNumNames; ++jj )
        {
            std::string lName = lpNameList;
            lpNameList += lName.length() + 1;
            size_t lNameHash = std::hash< std::string >{}( lName );

            int liPrevNum = liNum;
            for( int ii = 0; ii < miNumFiles; ++ii )
            {
                if( maFiles[ ii ].mNameHash == lNameHash )
                {
                    ++liNum;
                }
            }
            if( liNum == liPrevNum )
            {
                ++liNumIgnored;
            }
        }

        return liNum - liNumNames + liNumIgnored;
    }
    
    const sArkDef* GetArk( int liArkNum ) const    { return &maArks[ liArkNum ]; }
    const sFileDef* GetFile( int liFileNum ) const { return &maFiles[ liFileNum ]; }
    const sFileDef* GetFile( const char* lpPath, int liNum ) const
    {
        std::string lPath = lpPath;
        size_t lPathHash = std::hash< std::string >{}( lPath );

        const sFileDef* lpFileDef = maFiles;
        for( int ii = 0; ii < miNumFiles; ++ii, ++lpFileDef )
        {
            if( lpFileDef->mNameHash == lPathHash )
            {
                if( --liNum < 0 )
                {
                    return lpFileDef;
                }
            }
        }

        return nullptr;
    }

    const sFileDef* GetFileWithExtension( const char* lpExtension ) const;

    void SortFileList();
    void SortFileFlags2( int liFirst, int liLast );
};

class CEncryptedHeader
{
public:
    CEncryptedHeader();
    ~CEncryptedHeader();

    void SetReference( const CEncryptedHeader* lpReference )      {   mpReferenceHeader = lpReference;    }

    unsigned int Load( unsigned char* lpStream, unsigned int liStreamSize );
    unsigned char* Save( unsigned char* lpStream, unsigned int liSize, int liInitialKey, int liInitialKeyEncoded );
    bool Save( int liInitialKey, int liInitialKeyEncoded, bool lbEncrypt = true );

    void Construct( const char* lpInPath );

    void ExtractFiles( const char* lpOutPath );

    void Cycle( unsigned char* lpInStream, unsigned int liStreamSize );

    static int GenerateFileList( const char* lpDirectory, char*& lpScratchPtr, unsigned int* lpOutTotalFileSize );

private:
    void ExtractFile( int liIndex, const char* lpOutPath );
    int CycleKey( int liKey ) const;
    void UpdateKey();

    const CEncryptedHeader* mpReferenceHeader = nullptr;
    CHeaderData mData;
    unsigned char* mpArkData = nullptr;

    int miInitialKey;
    int miCurrentKey;

    long mlCurrentOffset;
    long mlKeyPos;
};

