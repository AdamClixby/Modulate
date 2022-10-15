// Modulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>
#include <functional>
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>

#include <deque>
#include <algorithm>
#include <string>
#include <cctype>

#include "Error.h"
#include "Settings.h"
#include "Utils.h"

#include "CArk.h"
#include "CDtaFile.h"
#include "CEncryptionCycler.h"

void ToUpper( std::string& lString )
{
    std::for_each( lString.begin(), lString.end(), []( char& c )
    {
        c = ::toupper( c );
    } );
}

void ToLower( std::string& lString )
{
    std::for_each( lString.begin(), lString.end(), []( char& c )
    {
        c = ::tolower( c );
    } );
}

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

eError EnablePackAll(std::deque< std::string >&)
{
    CSettings::mbPackAllFiles = true;

    VERBOSE_OUT("Enable packing for all files, even unused songs\n");

    return eError_NoError;
}

eError BuildSingleSong( std::deque< std::string >& laParams, bool lbDoOutput = true )
{
    if( lbDoOutput )
    {
        std::cout << "Building song ";
    }

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

    if( lbDoOutput )
    {
        std::cout << lSongName.c_str() << "\n";
    }

    std::string lFilename = lDirectory + CSettings::msPlatform + "/songs/" + lSongName + "/" + lSongName + ".moggsong";

    CMoggsong lDataFile;
    eError leError = lDataFile.LoadMoggSong( lFilename.c_str() );
    SHOW_ERROR_AND_RETURN;

    lDataFile.SetMoggPath( lSongName + ".mogg" );
    lDataFile.SetMidiPath( lSongName + ".mid" );

    std::string lOutFilename = lFilename + "_dta_" + CSettings::msPlatform;

    leError = lDataFile.Save( lOutFilename.c_str(), lbDoOutput );
    SHOW_ERROR_AND_RETURN;

    int liMidiBPM = (int)roundf( 60000000.0f / lDataFile.GetBPM() );

    std::string lMidiFilename = lDirectory + CSettings::msPlatform + "/songs/" + lSongName + "/" + lSongName + ".mid_" + CSettings::msPlatform;
    std::string lDonorPath = lDirectory + CSettings::msPlatform + "/songs/tut0";

#define COPY_DONOR(ext)                                                                                                   \
    {                                                                                                                     \
        std::string lDonorFile = lDonorPath + "/tut0." + ext;                                                             \
        std::string lDestFile = lDirectory + CSettings::msPlatform + "/songs/" + lSongName + "/" + lSongName + "." + ext; \
        if (FILE *file = fopen(lDestFile.c_str(), "r"))                                                                   \
        {                                                                                                                 \
            fclose(file);                                                                                                 \
        }                                                                                                                 \
        else                                                                                                              \
        {                                                                                                                 \
            std::ifstream src(lDonorFile.c_str(), std::ios::binary);                                                      \
            std::ofstream dst(lDestFile.c_str(), std::ios::binary);                                                       \
            dst << src.rdbuf();                                                                                           \
        }                                                                                                                 \
    };

    COPY_DONOR("mid_" + CSettings::msPlatform);
    COPY_DONOR("png.dta_dta_" + CSettings::msPlatform);
    COPY_DONOR("png_" + CSettings::msPlatform);

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

    int64_t lTerminator = CSettings::mbPS4 ? 0x01abcdabcdabcdab : 0xcdabcdabcdabcdab;
    unsigned char* lpDataPtr = lpMidiData + liMidiFileSize - 1024;
    while( lpDataPtr < lpMidiData + liMidiFileSize &&
            *lpDataPtr != 0xAB )
    {
        ++lpDataPtr;

        if( lpDataPtr == lpMidiData + liMidiFileSize )
        {
            std::cout << "Failed to update midi tempo from moggsong\n";
            return eError_InvalidData;
        }
    }

    if( *(int64_t*)lpDataPtr != lTerminator )
    {
        std::cout << "Failed to update midi tempo from moggsong\n";
        return eError_InvalidData;
    }

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

    if( lbDoOutput )
    {
        std::cout << "Writing " << lMidiFilename.c_str() << "\n";
    }

    fopen_s( &lpMidiFile, lMidiFilename.c_str(), "wb" );
    if( !lpMidiFile )
    {
        return eError_FailedToCreateFile;
    }

    fwrite( lpMidiData, 1, liMidiFileSize, lpMidiFile );
    fclose( lpMidiFile );

    delete[] lpMidiData;

    if( lbDoOutput )
    {
        std::cout << "\n";
    }

    return eError_NoError;
}

eError BuildSingleSongCommand( std::deque< std::string >& laParams )
{
    return BuildSingleSong( laParams );
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

    eError leError;
    std::vector< SSongConfig > lSongs;

    if (CSettings::mbPackAllFiles == true)
    {
        lSongs = std::vector< SSongConfig >();
    }
    else
    {
        std::string lAmpConfigPath = lInputPath + CSettings::msPlatform + "/config/amp_config.dta_dta_" + CSettings::msPlatform;
        std::cout << "Loading " << lAmpConfigPath.c_str() << "\n";

        CDtaFile lAmpConfig;
        leError = lAmpConfig.Load( lAmpConfigPath.c_str() );
        SHOW_ERROR_AND_RETURN;

        lSongs = lAmpConfig.GetSongs();
        CDtaFile lSongsConfig;

        std::string lAmpSongsConfigPath = lInputPath + CSettings::msPlatform + "/config/amp_songs_config.dta_dta_" + CSettings::msPlatform;
        leError = lSongsConfig.Load( lAmpSongsConfigPath.c_str() );
        SHOW_ERROR_AND_RETURN;

        lSongsConfig.GetSongData( lSongs );
    }

    CArk lReferenceArkHeader;
    std::string lHeaderFilename = std::string( "main_" ).append( CSettings::msPlatform ).append( ".hdr" );
    leError = lReferenceArkHeader.Load( lHeaderFilename.c_str() );
    //lReferenceArkHeader.LoadArkData();
    SHOW_ERROR_AND_RETURN;

    CArk lArkHeader;
    leError = lArkHeader.ConstructFromDirectory( lInputPath.c_str(), lReferenceArkHeader, lSongs );
    SHOW_ERROR_AND_RETURN;

    lArkHeader.BuildArk( lInputPath.c_str(), lSongs );
    lArkHeader.SaveArk( lOuptutPath.c_str(), lHeaderFilename.c_str() );
    //lReferenceArkHeader.SaveArk( lOuptutPath.c_str(), lHeaderFilename.c_str() );

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

    size_t liWritten = fwrite( lpHeaderData, 1, liHeaderSize, lpHeaderFile );
    fclose( lpHeaderFile );

    delete[] lpHeaderData;

    return liWritten == (size_t)liHeaderSize ? eError_NoError : eError_FailedToWriteData;
}

eError ListSongs( std::deque< std::string >& laParams )
{
    std::cout << "Loading ";
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

    std::string lAmpConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_config.dta_dta_" + CSettings::msPlatform;
    std::cout << lAmpConfigPath.c_str() << "\n";

    CDtaFile lAmpConfig;
    eError leError = lAmpConfig.Load( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::string lAmpSongsConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_songs_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpSongsConfigPath.c_str() << "\n";

    CDtaFile lSongsConfig;
    leError = lSongsConfig.Load( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::cout << "\n";

    int ii = 1;
    std::vector< SSongConfig > lSongs = lAmpConfig.GetSongs();
    lSongsConfig.GetSongData( lSongs );
    for( auto& lSong : lSongs )
    {
        std::cout << "Song " << ii << "\t  " << lSong.mId << " - " << lSong.mName << " - " << lSong.mType << "\n\t  " << lSong.mPath << "\n\t  Unlocked by " << lSong.mUnlockMethod << " " << lSong.miUnlockCount << "\n" << "\t  Arena: " << lSong.mArena << "\n\n";
        ++ii;
    }

    //lAmpConfig.Save( lAmpConfigPath.c_str() );
    //lSongsConfig.Save( lAmpSongsConfigPath.c_str() );

    return eError_NoError;
}

void PrintUsage()
{
    std::cout << "Usage: Modulate.exe <options> <commands>\n\n";
    std::cout << "Options:\n";
    std::cout << "-verbose\tShow additional information during operation\n";
    std::cout << "-force\t\tOverwrite existing files\n";
    std::cout << "-packall\t\tPack all files, even unused song files\n";
    std::cout << "\nCommands:\n";
    std::cout << "-unpack <out_dir>\t\t\t\tUnpack the contents of the .ark file(s) to <out_dir>\n";
    std::cout << "-pack <in_dir> <out_dir>\t\t\tRepack data into an .ark file, ignoring new files\n";
    std::cout << "-pack_add <in_dir> <out_dir>\t\t\tRepack data into an .ark file, adding new files\n";
    std::cout << "-replace <data_dir> <old_song> <new_song>\tReplace <old_song> with the song from <new_song>\n\t\t\t\t\t\tRequires <new_song.mid> <new_song.mogg> <new_song.moggsong>\n";
    std::cout << "-buildsong <data_dir> <song_name>\t\tBuild <song_name> binary data from <song_name.moggsong>\n";
    std::cout << "-buildsongs <data_dir>\t\t\t\tBuild binary data for all songs from their .moggsong (tutorials excluded)\n";
    std::cout << "-decode <data_dir>\t\t\t\tDecode the .hdr file\n";
    std::cout << "-listsongs <data_dir>\t\t\t\tList all songs from the game\n";
    std::cout << "-addsong <data_dir> <song_name>\t\t\tAdd <song_name> to the list of songs\n";
    std::cout << "-removesong <data_dir> <song_name>\t\tRemove <song_name> from the list of songs\n";
    std::cout << "-autoadd <data_dir>            \t\t\tAdd all new songs in the songs folder to the list of songs\n";
}

eError PS3( std::deque< std::string >& laParams )
{
    CSettings::mbPS4 = false;
    CSettings::msPlatform = "ps3";

    std::cout << "Switching to PS3 mode\n";

    return eError_NoError;
}

eError AddPack( std::deque< std::string >& laParams )
{
    CSettings::mbIgnoreNewFiles = false;
    eError leError = Pack( laParams );
    CSettings::mbIgnoreNewFiles = true;
    return leError;
}

eError AddSong( std::deque< std::string >& laParams )
{
    std::deque< std::string > laBuildSongParams = laParams;
    BuildSingleSong( laBuildSongParams );

    std::cout << "Adding song ";

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

    std::string lSongId = laParams.front();
    std::cout << lSongId << "\n";

    std::string lAmpConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpConfigPath.c_str() << "\n";

    CDtaFile lAmpConfig;
    eError leError = lAmpConfig.Load( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::string lAmpSongsConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_songs_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpSongsConfigPath.c_str() << "\n";

    CDtaFile lSongsConfig;
    leError = lSongsConfig.Load( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::vector< SSongConfig > laSongs = lAmpConfig.GetSongs();
    lSongsConfig.GetSongData( laSongs );

    laParams.pop_front();

    std::string lMoggFilename = lBasePath + CSettings::msPlatform + "/songs/" + lSongId + "/" + lSongId + ".moggsong";
    
    std::cout << "Loading " << lMoggFilename.c_str() << "\n";

    CMoggsong lMoggFile;
    leError = lMoggFile.LoadMoggSong( lMoggFilename.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::string lSongIdUpper = lSongId;
    ToUpper( lSongIdUpper );

    SSongConfig lNewSong;
    lNewSong.mId = lSongIdUpper;
    lNewSong.mName = lMoggFile.GetTitle();
    lNewSong.mUnlockMethod = "play_num";
    lNewSong.mType = "kSongExtra";
    lNewSong.mPath = "../songs/" + lSongId + "/" + lSongId + ".moggsong";
    lNewSong.miUnlockCount = 0;
    lNewSong.mArena = lMoggFile.GetArenaName();

    laSongs.push_back( lNewSong );

    std::cout << "Updating song data\n";

    leError = lSongsConfig.UpdateSongData( laSongs );
    SHOW_ERROR_AND_RETURN;

    leError = lAmpConfig.SetSongs( laSongs );
    SHOW_ERROR_AND_RETURN;

    std::cout << "Saving " << lAmpConfigPath.c_str() << "\n";

    leError = lAmpConfig.Save( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::cout << "Saving " << lAmpSongsConfigPath.c_str() << "\n";

    leError = lSongsConfig.Save( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    return eError_NoError;
}

eError AutoAddAllSongs( std::deque< std::string >& laParams )
{
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

    std::string lAmpConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpConfigPath.c_str() << "\n";

    CDtaFile lAmpConfig;
    eError leError = lAmpConfig.Load( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::string lAmpSongsConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_songs_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpSongsConfigPath.c_str() << "\n";

    CDtaFile lSongsConfig;
    leError = lSongsConfig.Load( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::cout << "\nAdding all songs from " << lBasePath.c_str() << CSettings::msPlatform << "/songs/\n";

    WIN32_FIND_DATAA lFindData;
    std::string lSearchPath = lBasePath + CSettings::msPlatform + "/songs/*.*";
    HANDLE lFindHandle = FindFirstFileA( lSearchPath.c_str(), &lFindData );

    std::vector< SSongConfig > laSongs = lAmpConfig.GetSongs();
    lSongsConfig.GetSongData( laSongs );

    std::vector< std::string > laExistingSongNames;
    for( const SSongConfig& lExistingSong : laSongs )
    {
        size_t liStartPos = lExistingSong.mPath.find( '/', 3 );
        if( liStartPos == std::string::npos )
        {
            leError = eError_InvalidData;
            SHOW_ERROR_AND_RETURN;
        }

        size_t liEndPos = lExistingSong.mPath.find( '/', 3 + liStartPos );
        if( liEndPos == std::string::npos )
        {
            leError = eError_InvalidData;
            SHOW_ERROR_AND_RETURN;
        }

        std::string lSongName = lExistingSong.mPath.substr( liStartPos + 1, liEndPos - liStartPos - 1 );
        ToUpper( lSongName );
        laExistingSongNames.push_back( lSongName );
    }

    int liSongsAdded = 0;
    do
    {
        if( ( lFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
        {
            continue;
        }

        std::string lDirectoryName = lFindData.cFileName;
        ToUpper( lDirectoryName );

        if( lDirectoryName == "."    || lDirectoryName == ".."   || lDirectoryName == "CREDITS" ||
            lDirectoryName == "TUT0" || lDirectoryName == "TUT1" || lDirectoryName == "TUT2" || lDirectoryName == "TUTC" )
        {
            continue;
        }

        bool lbAlreadyInSongList = false;
        for( const std::string& lExistingSong : laExistingSongNames )
        {
            if( lExistingSong == lDirectoryName )
            {
                lbAlreadyInSongList = true;
                break;
            }
        }

        if( lbAlreadyInSongList )
        {
            continue;
        }

        std::string lSongName = lDirectoryName;
        ToLower( lSongName );
        std::cout << "Loading " << lSongName << "... ";

        std::string lFilename = lBasePath + CSettings::msPlatform + "/songs/" + lDirectoryName + "/" + lDirectoryName + ".moggsong";

        CMoggsong lDataFile;
        eError leError = lDataFile.LoadMoggSong( lFilename.c_str() );
        if( leError != eError_NoError )
        {
            continue;
        }

        std::deque< std::string > lSingleSongParams;
        lSingleSongParams.push_back( lBasePath );
        lSingleSongParams.push_back( lSongName );

        std::cout << "building... ";
        leError = BuildSingleSong( lSingleSongParams, false );
        if( leError != eError_NoError )
        {
            continue;
        }
        std::cout << "done!\n";

        std::string lMoggFilename = lBasePath + CSettings::msPlatform + "/songs/" + lSongName + "/" + lSongName + ".moggsong";

        CMoggsong lMoggFile;
        leError = lMoggFile.LoadMoggSong( lMoggFilename.c_str() );
        SHOW_ERROR_AND_RETURN;

        SSongConfig lNewSong;
        lNewSong.mId = lSongName;
        lNewSong.mName = lMoggFile.GetTitle();
        lNewSong.mUnlockMethod = "play_num";
        lNewSong.mType = "kSongExtra";
        lNewSong.mPath = "../songs/" + lSongName + "/" + lSongName + ".moggsong";
        lNewSong.miUnlockCount = 0;
        lNewSong.mArena = lMoggFile.GetArenaName();
        ToUpper( lNewSong.mId );

        laSongs.push_back( lNewSong );
        ++liSongsAdded;
    } while( FindNextFileA( lFindHandle, &lFindData ) != 0 );

    FindClose( lFindHandle );

    if( liSongsAdded == 0 )
    {
        std::cout << "Found no new songs\n";
        return eError_NoError;
    }

    leError = lSongsConfig.UpdateSongData( laSongs );
    SHOW_ERROR_AND_RETURN;

    leError = lAmpConfig.SetSongs( laSongs );
    SHOW_ERROR_AND_RETURN;

    std::cout << "Saving " << lAmpConfigPath.c_str() << "\n";

    leError = lAmpConfig.Save( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::cout << "Saving " << lAmpSongsConfigPath.c_str() << "\n";

    leError = lSongsConfig.Save( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::cout << "Added " << liSongsAdded << " new songs\n";

    return eError_NoError;
}

eError RemoveSong( std::deque< std::string >& laParams )
{
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

    std::string lAmpConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpConfigPath.c_str() << "\n";

    CDtaFile lAmpConfig;
    eError leError = lAmpConfig.Load( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::string lAmpSongsConfigPath = lBasePath + CSettings::msPlatform + "/config/amp_songs_config.dta_dta_" + CSettings::msPlatform;
    std::cout << "Loading " << lAmpSongsConfigPath.c_str() << "\n";

    CDtaFile lSongsConfig;
    leError = lSongsConfig.Load( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::string lSongId = laParams.front();
    laParams.pop_front();

    std::transform( lSongId.begin(), lSongId.end(), lSongId.begin(), []( unsigned char c ) {
        return std::toupper( c );
    } );

    std::cout << "Removing song " << lSongId << "\n";

    bool lbRemovedFromSongList = lSongsConfig.RemoveSong( lSongId );
    bool lbRemovedFromConfig = lAmpConfig.RemoveSong( lSongId );

    if( !lbRemovedFromConfig || !lbRemovedFromSongList )
    {
        std::cout << "Failed to find song " << lSongId << " in song list\n";
        leError = eError_InvalidParameter;
        SHOW_ERROR_AND_RETURN;
    }

    std::cout << "Saving " << lAmpConfigPath.c_str() << "\n";

    leError = lAmpConfig.Save( lAmpConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    std::cout << "Saving " << lAmpSongsConfigPath.c_str() << "\n";

    leError = lSongsConfig.Save( lAmpSongsConfigPath.c_str() );
    SHOW_ERROR_AND_RETURN;

    return eError_NoError;
}

int main( int argc, char *argv[], char *envp[] )
{
    struct sCommandPair
    {
        const char* mpCommandName;
        std::function<eError( std::deque< std::string >& )> mFunction;
    };
    const sCommandPair kaCommands[] = {
        "-ps3",         PS3,
        "-verbose",     EnableVerbose,
        "-force",       EnableForceWrite,
        "-packall",     EnablePackAll,
        "-unpack",      Unpack,
        "-replace",     ReplaceSong,
        "-buildsong",   BuildSingleSongCommand,
        "-buildsongs",  BuildSongs,
        "-pack",        Pack,
        "-pack_add",    AddPack,
        "-decode",      Decode,
        "-listsongs",   ListSongs,
        "-addsong",     AddSong,
        "-autoadd",     AutoAddAllSongs,
        "-removesong",  RemoveSong,
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
