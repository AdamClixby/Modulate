// Modulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>
#include <functional>

#include <deque>
#include <string>

#include "Error.h"
#include "Settings.h"
#include "Utils.h"

#include "CArk.h"
#include "CDtaFile.h"
#include "CEncryptionCycler.h"

eError EnableVerbose( std::deque< std::string >& )
{
    CSettings::mbVerbose = true;

    std::cout << "Verbose mode enabled\n";

    return eError_NoError;
}

eError EnableForceWrite( std::deque< std::string >& )
{
    CSettings::mbOverwriteOutputFiles = true;

    VERBOSE_OUT( "Overwriting existing files when necessary\n" );

    return eError_NoError;
}

eError BuildSingleSong( std::deque< std::string >& laParams )
{
    std::cout << "Building song ";
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }

    std::string lDirectory = laParams.front();
    laParams.pop_front();

    if( lDirectory.back() != '/' && lDirectory.back() != '\\' )
    {
        lDirectory += '/';
    }
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }

    std::string lSongName = laParams.front();
    laParams.pop_front();
    std::cout << lSongName.c_str() << "\n";

    std::string lFilename = lDirectory + CSettings::msPlatform + "/songs/" + lSongName + "/" + lSongName + ".moggsong";

    CDtaFile lDataFile;
    eError leError = lDataFile.LoadMoggSong( lFilename.c_str() );
    SHOW_ERROR_AND_RETURN;

    lDataFile.SetMoggPath( lSongName + ".mogg" );
    lDataFile.SetMidiPath( lSongName + ".mid" );

    std::string lOutFilename = lFilename + "_dta_" + CSettings::msPlatform;

    leError = lDataFile.Save( lOutFilename.c_str() );
    SHOW_ERROR_AND_RETURN;

    int liMidiBPM = (int)roundf( 60000000.0f / lDataFile.GetBPM() );

    std::string lMidiFilename = lDirectory + CSettings::msPlatform + "/songs/" + lSongName + "/" + lSongName + ".mid_" + CSettings::msPlatform;
    FILE* lpMidiFile = nullptr;
    fopen_s( &lpMidiFile, lMidiFilename.c_str(), "rb" );
    if( !lpMidiFile )
    {
        return eError_FailedToOpenFile;
    }

    fseek( lpMidiFile, 0, SEEK_END );
    int liMidiFileSize = ftell( lpMidiFile );
    fseek( lpMidiFile, 0, SEEK_SET );

    unsigned char* lpMidiData = new unsigned char[ liMidiFileSize ];
    fread( lpMidiData, liMidiFileSize, 1, lpMidiFile );
    fclose( lpMidiFile );

    if( CSettings::mbPS4 )
    {
    }
    else
    {
        // Is this necessary?
        lpMidiData[ 0x2A ] = ( liMidiBPM >> 16 ) & 0xFF;
        lpMidiData[ 0x2B ] = ( liMidiBPM >> 8 ) & 0xFF;
        lpMidiData[ 0x2C ] = ( liMidiBPM >> 0 ) & 0xFF;
    }

    int64_t lTerminator = CSettings::mbPS4 ? 0x01abcdabcdabcdab : 0xcdabcdabcdabcdab;
    unsigned char* lpDataPtr = lpMidiData + liMidiFileSize - 1024;
    do
    {
        while( lpDataPtr < lpMidiData + liMidiFileSize &&
              *lpDataPtr != 0xAB )
        {
            ++lpDataPtr;
        }

        if( lpDataPtr == lpMidiData + liMidiFileSize )
        {
            return eError_InvalidData;
        }
    } while( lpDataPtr >= lpMidiData && *(int64_t*)lpDataPtr != lTerminator );

    lpDataPtr += 19;
    if( CSettings::mbPS4 )
    {
        lpDataPtr[ 0 ] = ( liMidiBPM >> 0 ) & 0xFF;
        lpDataPtr[ 1 ] = ( liMidiBPM >> 8 ) & 0xFF;
        lpDataPtr[ 2 ] = ( liMidiBPM >> 16 ) & 0xFF;
    }
    else
    {
        lpDataPtr += 2;

        lpDataPtr[ 0 ] = ( liMidiBPM >> 16 ) & 0xFF;
        lpDataPtr[ 1 ] = ( liMidiBPM >> 8 ) & 0xFF;
        lpDataPtr[ 2 ] = ( liMidiBPM >> 0 ) & 0xFF;
    }

    std::cout << "Writing " << lMidiFilename.c_str() << "\n";

    fopen_s( &lpMidiFile, lMidiFilename.c_str(), "wb" );
    if( !lpMidiFile )
    {
        return eError_FailedToCreateFile;
    }

    fwrite( lpMidiData, liMidiFileSize, 1, lpMidiFile );
    fclose( lpMidiFile );

    delete[] lpMidiData;

    return eError_NoError;
}

eError BuildSongs( std::deque< std::string >& laParams )
{
    std::cout << "Building songs at ";
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }

    std::string lDirectory = laParams.front();
    laParams.pop_front();

    if( lDirectory.back() != '/' && lDirectory.back() != '\\' )
    {
        lDirectory += '/';
    }
    std::cout << lDirectory.c_str() << CSettings::msPlatform << "/songs/\n";

    CArk lReferenceArkHeader;
    std::string lHeaderFilename = std::string( "main_" ).append( CSettings::msPlatform ).append( ".hdr" );
    eError leError = lReferenceArkHeader.Load( lHeaderFilename.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::vector< std::string > laFilenames;
    laFilenames.reserve( lReferenceArkHeader.GetNumFiles() );

    const char* lpMoggSong = ".moggsong";
    const size_t kiMoggSongStrLen = strlen( lpMoggSong );

    CUtils::GenerateFileList( lReferenceArkHeader, lDirectory.c_str(), laFilenames, lpMoggSong );
    for( std::vector< std::string >::const_iterator lIter = laFilenames.begin(); lIter != laFilenames.end(); ++lIter )
    {
        const std::string& lFilename = *lIter;
        const char* lpExtension = strstr( lFilename.c_str(), lpMoggSong );
        if( lpExtension != ( lFilename.c_str() + strlen( lFilename.c_str() ) - kiMoggSongStrLen ) )
        {
            continue;
        }

        const char* kpSearchString = "/songs/";
        const char* lpSongName = strstr( lFilename.c_str(), kpSearchString );
        if( !lpSongName )
        {
            continue;
        }
        lpSongName += strlen( kpSearchString );

        const char* lpSongNameEnd = strstr( lpSongName, "/" );
        if( !lpSongNameEnd )
        {
            return eError_InvalidParameter;
        }

        if( strstr( lpSongName, "tut" ) == lpSongName && ( lpSongNameEnd - lpSongName ) == 4 )
        {
            continue;
        }

        std::string lSongName( lpSongName );
        lSongName = lSongName.substr( 0, lpSongNameEnd - lpSongName );

        std::deque< std::string > laSingleSongParam = { lDirectory, lSongName };
        eError leError = BuildSingleSong( laSingleSongParam );
        SHOW_ERROR_AND_RETURN;
    }

    return eError_NoError;
}

eError Unpack( std::deque< std::string >& laParams )
{
    std::cout << "Unpacking main_" << CSettings::msPlatform << ".hdr to ";
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }

    std::string lOutputDirectory = laParams.front();
    laParams.pop_front();

    std::cout << lOutputDirectory.c_str() << "\n";
    if( lOutputDirectory.back() != '/' && lOutputDirectory.back() != '\\' )
    {
        lOutputDirectory += "/";
    }

    CArk lArkHeader;
    std::string lHeaderFilename = std::string( "main_" ).append( CSettings::msPlatform ).append( ".hdr" );
    eError leError = lArkHeader.Load( lHeaderFilename.c_str() );
    SHOW_ERROR_AND_RETURN;

    leError = lArkHeader.ExtractFiles( 0, lArkHeader.GetNumFiles(), lOutputDirectory.c_str() );
    SHOW_ERROR_AND_RETURN;

    return eError_NoError;
}

eError ReplaceSong( std::deque< std::string >& laParams )
{
    std::cout << "Replacing song ";
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }
    std::string lBasePath = laParams.front();
    laParams.pop_front();
    if( lBasePath.back() != '/' && lBasePath.back() != '\\' )
    {
        lBasePath += "/";
    }

    std::cout << lBasePath.c_str();
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }
    std::string lOldSong = laParams.front();
    laParams.pop_front();

    std::cout << lOldSong.c_str() << " with " << lBasePath.c_str();
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }
    std::string lNewSong = laParams.front();
    laParams.pop_front();

    std::cout << lNewSong.c_str() << "\n";

    std::string lSourcePathBase = lBasePath + CSettings::msPlatform + "/songs/" + lNewSong + "/" + lNewSong;
    std::string lTargetPathBase = lBasePath + CSettings::msPlatform + "/songs/" + lOldSong + "/" + lOldSong;

    auto lCopyFile = [ & ]( const char* lpExtension )
    {
        std::string lSourcePath = lSourcePathBase + lpExtension;
        std::string lTargetPath = lTargetPathBase + lpExtension;

        std::cout << lSourcePath << " --> " << lTargetPath.c_str() << "\n";
        if( !CopyFileA( lSourcePath.c_str(), lTargetPath.c_str(), FALSE ) )
        {
            return eError_FailedToCopyFile;
        }

        return eError_NoError;
    };

    eError leError = lCopyFile( ".mid     " );   SHOW_ERROR_AND_RETURN;
    leError = lCopyFile( ".mogg    " );          SHOW_ERROR_AND_RETURN;
    leError = lCopyFile( ".moggsong" );          SHOW_ERROR_AND_RETURN;

    std::string lTarget = lTargetPathBase + ".moggsong";

    std::deque< std::string > lSingleSongParams = { lBasePath, lOldSong };
    BuildSingleSong( lSingleSongParams );

    return eError_NoError;
}

eError Pack( std::deque< std::string >& laParams )
{
    std::cout << "Packing main_" << CSettings::msPlatform << ".hdr from ";
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }

    std::string lInputPath = laParams.front();
    laParams.pop_front();
    std::cout << lInputPath.c_str() << " to ";
    if( laParams.empty() )
    {
        return eError_InvalidParameter;
    }

    std::string lOuptutPath = laParams.front();
    laParams.pop_front();
    std::cout << lOuptutPath.c_str() << "\n";

    if( lInputPath.back() != '/' && lInputPath.back() != '\\' )
    {
        lInputPath += "/";
    }
    if( lOuptutPath.back() != '/' && lOuptutPath.back() != '\\' )
    {
        lOuptutPath += "/";
    }

    CArk lReferenceArkHeader;
    std::string lHeaderFilename = std::string( "main_" ).append( CSettings::msPlatform ).append( ".hdr" );
    eError leError = lReferenceArkHeader.Load( lHeaderFilename.c_str() );
    lReferenceArkHeader.LoadArkData();
    SHOW_ERROR_AND_RETURN;

    //CArk lArkHeader;
    //leError = lArkHeader.ConstructFromDirectory( lInputPath.c_str(), lReferenceArkHeader );
    //SHOW_ERROR_AND_RETURN;

    //lArkHeader.BuildArk( lInputPath.c_str() );
    //lArkHeader.SaveArk( lOuptutPath.c_str(), lHeaderFilename.c_str() );
    lReferenceArkHeader.SaveArk( lOuptutPath.c_str(), lHeaderFilename.c_str() );

    return eError_NoError;
}

eError Decode( std::deque< std::string >& laParams )
{
    std::string lHeaderFilename = std::string( "main_" ).append( CSettings::msPlatform ).append( ".hdr" );
    VERBOSE_OUT( "Loading header file " << lHeaderFilename.c_str() );

    const unsigned int kuUnencryptedVersion = 9;
    FILE* lpHeaderFile = nullptr;
    fopen_s( &lpHeaderFile, lHeaderFilename.c_str(), "rb" );
    if( !lpHeaderFile )
    {
        return eError_FailedToOpenFile;
    }

    fseek( lpHeaderFile, 0, SEEK_END );
    unsigned int liHeaderSize = ftell( lpHeaderFile );
    unsigned char* lpHeaderData = new unsigned char[ liHeaderSize ];

    fseek( lpHeaderFile, 0, SEEK_SET );
    fread( lpHeaderData, liHeaderSize, 1, lpHeaderFile );
    fclose( lpHeaderFile );

    VERBOSE_OUT( "\nLoaded header (" << liHeaderSize << ") bytes\n" );

    unsigned int luVersion = *(unsigned int*)( lpHeaderData );
    if( luVersion != CSettings::kuEncryptedVersionPS3 &&
        luVersion != CSettings::kuEncryptedVersionPS4 )
    {
        delete[] lpHeaderData;
        return eError_UnknownVersionNumber;
    }

    const unsigned int kuInitialKey = ( luVersion == CSettings::kuEncryptedVersionPS3 ) ? CSettings::kuEncryptedPS3Key : CSettings::kuEncryptedPS4Key;

    CEncryptionCycler lDecrypt;
    lDecrypt.Cycle( lpHeaderData + sizeof( unsigned int ), liHeaderSize - sizeof( unsigned int ), kuInitialKey );

    lHeaderFilename.append( ".dec" );
    fopen_s( &lpHeaderFile, lHeaderFilename.c_str(), "wb" );
    if( !lpHeaderFile )
    {
        delete[] lpHeaderData;
        return eError_FailedToCreateFile;
    }
    fwrite( lpHeaderData, 1, liHeaderSize, lpHeaderFile );
    fclose( lpHeaderFile );

    delete[] lpHeaderData;

    return eError_NoError;
}

void PrintUsage()
{
    std::cout << "Usage: Modulate.exe <options> <commands>\n\n";
    std::cout << "Options:\n";
    std::cout << "-verbose\tShow additional information during operation\n";
    std::cout << "-force\t\tOverwrite existing files\n";
    std::cout << "\nCommands:\n";
    std::cout << "-unpack <out_dir>\t\t\t\tUnpack the contents of the .ark file(s) to <out_dir>\n";
    std::cout << "-pack <in_dir> <out_dir>\t\t\tRepack data into an .ark file\n";
    std::cout << "-replace <data_dir> <old_song> <new_song>\tReplace <old_song> with the song from <new_song>\n\t\t\t\t\t\tRequires <new_song.mid> <new_song.mogg> <new_song.moggsong>\n";
}

int main( int argc, char *argv[], char *envp[] )
{
    struct sCommandPair
    {
        const char* mpCommandName;
        std::function<eError( std::deque< std::string >& )> mFunction;
    };
    const sCommandPair kaCommands[] = {
        "-verbose",     EnableVerbose,
        "-force",       EnableForceWrite,
        "-unpack",      Unpack,
        "-replace",     ReplaceSong,
        "-buildsong",   BuildSingleSong,
        "-buildsongs",  BuildSongs,
        "-pack",        Pack,
        "-decode",      Decode,
        nullptr,        nullptr,
    };

    std::deque< std::string > laParams;
    for( int ii = 1; ii < argc; ++ii )
    {
        laParams.push_back( argv[ ii ] );
    }

    if( laParams.empty() )
    {
        PrintUsage();
        return 0;
    }

    while( !laParams.empty() )
    {
        size_t liNumParams = laParams.size();

        const sCommandPair* lpCommandPair = kaCommands;
        while( lpCommandPair->mFunction )
        {
            if( _stricmp( laParams.front().c_str(), lpCommandPair->mpCommandName ) != 0 )
            {
                ++lpCommandPair;
                continue;
            }

            laParams.pop_front();
            eError leError = lpCommandPair->mFunction( laParams );
            switch( leError )
            {
            case eError_NoError:
                std::cout << "\n";
                break;
            default:
                ShowError( leError );
                return -1;
            }

            break;
        }

        if( liNumParams == laParams.size() )
        {
            std::cout << "Unkown parameter: " << laParams.front().c_str() << "\nAborting\n\n";
            PrintUsage();
            return -1;
        }
    }

    std::cout << "Complete!\n";

    return 0;
}
