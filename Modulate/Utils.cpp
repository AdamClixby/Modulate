#include "pch.h"
#include "Utils.h"
#include "CArk.h"

int CUtils::GenerateFileList( const CArk& lReferenceHeader, const char* lpDirectory, std::vector<std::string>& laFilenames, const char* lpStringToMatch )
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
            if( lpStringToMatch && !strstr( lFindFileData.cFileName, lpStringToMatch ) )
            {
                continue;
            }

            std::string lSourceFilename = lpDirectory + std::string( lFindFileData.cFileName );
            std::string lArkFilename = ( strstr( lpDirectory, "/" ) + 1 ) + std::string( lFindFileData.cFileName );
            if( !lReferenceHeader.FileExists( lArkFilename.c_str() ) )
            {
                //VERBOSE_OUT( "Skipping unknown file: " << lSourceFilename.c_str() << "\n" );
                //continue;
            }

            FILE* lFile = nullptr;
            fopen_s( &lFile, lSourceFilename.c_str(), "rb" );
            if( !lFile )
            {
                std::cout << "Unable to open file: " << lSourceFilename.c_str() << "\n";
                continue;
            }

            //VERBOSE_OUT( "Adding file: " << lSourceFilename.c_str() << "\n" );
            laFilenames.push_back( lArkFilename );

            fseek( lFile, 0, SEEK_END );
            int liFileSize = ftell( lFile );
            fclose( lFile );

            ++liNumFiles;
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

            liNumFiles += GenerateFileList( lReferenceHeader, lSearchString.c_str(), laFilenames, lpStringToMatch );
        }
    } while( FindNextFileA( lFindFileHandle, &lFindFileData ) != 0 );

    return liNumFiles;
}
