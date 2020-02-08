// Modulate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <sys/types.h>
#include <direct.h>
#include <windows.h>
#include <functional>

#include "Utils.h"
#include "CEncryptedHeader.h"
#include "CAmpConfig.h"

#include "Error.h"
#include "Settings.h"
#include "CArk.h"
#include "CDtaFile.h"

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

eError EnableVerbose( const char* )
{
    CSettings::mbVerbose = true;

    std::cout << "Verbose mode enabled\n";

    return eError_NoError;
}

eError EnableForceWrite( const char* )
{
    CSettings::mbOverwriteOutputFiles = true;

    VERBOSE_OUT( "Overwriting existing files when necessary\n" );

    return eError_NoError;
}

eError SetInputFile( const char* lpInputFilename )
{
    bool lbValidFile = false;

    VERBOSE_OUT( "Setting input file to " << lpInputFilename << "\n" );

    FILE* lpInputFile = nullptr;
    fopen_s( &lpInputFile, lpInputFilename, "rb" );
    lbValidFile = ( lpInputFile != nullptr );
    fclose( lpInputFile );

    if( !lbValidFile )
    {
        return eError_FailedToOpenFile;
    }

    CSettings::mpInputFilename = lpInputFilename;

    return eError_NoError;
}

eError SetOutputFile( const char* lpOutputFilename )
{
    VERBOSE_OUT( "Setting output file to " << lpOutputFilename << "\n" );

    CSettings::mpOutputFilename = lpOutputFilename;

    return eError_NoError;
}

eError SetInputDirectory( const char* lpInputDirectoryName )
{
    VERBOSE_OUT( "Setting input directory to " << lpInputDirectoryName << "\n" );

    std::string lNewInputDirectory = lpInputDirectoryName;
    if( lNewInputDirectory.back() == '/' )
    {
        CSettings::mInputDirectory = lNewInputDirectory;
    }
    else
    {
        CSettings::mInputDirectory = lNewInputDirectory + "/";
    }
    VERBOSE_OUT( "Input directory set to " << CSettings::mInputDirectory.c_str() << "\n" );

    return eError_NoError;
}

eError BuildSongs( const char* )
{
    CArk lReferenceArkHeader;
    eError leError = lReferenceArkHeader.Load( CSettings::mpInputFilename );
    SHOW_ERROR_AND_RETURN;

    std::vector< std::string > laFilenames;
    laFilenames.reserve( lReferenceArkHeader.GetNumFiles() );

    const char* lpMoggSong = ".moggsong";
    const size_t kiMoggSongStrLen = strlen( lpMoggSong );

    CUtils::GenerateFileList( lReferenceArkHeader, CSettings::mInputDirectory.c_str(), laFilenames, lpMoggSong );
    for( std::vector< std::string >::const_iterator lIter = laFilenames.begin(); lIter != laFilenames.end(); ++lIter )
    {
        const std::string& lFilename = *lIter;
        const char* lpExtension = strstr( lFilename.c_str(), lpMoggSong );
        if( lpExtension != ( lFilename.c_str() + strlen( lFilename.c_str() ) - kiMoggSongStrLen ) )
        {
            continue;
        }

        if( strstr( lFilename.c_str(), "wetware" ) == nullptr )
        {
            continue;
        }

        std::cout << "Compiling " << lFilename.c_str() << "\n";

        CDtaFile lDataFile;
        lDataFile.Load( "" );

        std::string lOutFilename = CSettings::mInputDirectory + lFilename;
        lOutFilename.append( "_dta_ps3" );
        lDataFile.Save( lOutFilename.c_str() );
    }

    return eError_NoError;
}

eError SetOutputDirectory( const char* lpOutputDirectoryName )
{
    bool lbValidDir = false;

    VERBOSE_OUT( "Setting output directory to " << lpOutputDirectoryName << "\n" );

    std::string lNewOutputDirectory = lpOutputDirectoryName;
    if( lNewOutputDirectory.back() == '/' )
    {
        lNewOutputDirectory = lNewOutputDirectory.substr( 0, lNewOutputDirectory.length() - 1 );
    }

    WIN32_FIND_DATAA lFindFileData;
    HANDLE lFindFileHandle = FindFirstFileA( lNewOutputDirectory.c_str(), &lFindFileData );
    if( ( lFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
    {
        VERBOSE_OUT( "Directory does not exist, creating\n" );
        _mkdir( lNewOutputDirectory.c_str() );
    }
    
    lFindFileHandle = FindFirstFileA( lNewOutputDirectory.c_str(), &lFindFileData );
    if( ( lFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 )
    {
        return eError_FailedToCreateDirectory;
    }

    CSettings::mOutputDirectory = lNewOutputDirectory + "/";
    VERBOSE_OUT( "Output directory set to " << CSettings::mOutputDirectory.c_str() << "\n" );

    return eError_NoError;
}

eError Unpack( const char* )
{
    std::cout << "Unpacking " << CSettings::mpInputFilename << " to " << CSettings::mOutputDirectory.c_str() << "\n";

    CArk lArkHeader;
    eError leError = lArkHeader.Load( CSettings::mpInputFilename );
    SHOW_ERROR_AND_RETURN;

    leError = lArkHeader.ExtractFiles( 0, lArkHeader.GetNumFiles(), CSettings::mOutputDirectory.c_str() );
    SHOW_ERROR_AND_RETURN;

    return eError_NoError;
}

eError Pack( const char* )
{
    std::cout << "Packing " << CSettings::mpOutputFilename << " from " << CSettings::mInputDirectory.c_str() << "\n";

    CArk lReferenceArkHeader;
    eError leError = lReferenceArkHeader.Load( CSettings::mpInputFilename );
    SHOW_ERROR_AND_RETURN;

    CArk lArkHeader;
    leError = lArkHeader.ConstructFromDirectory( CSettings::mInputDirectory.c_str(), lReferenceArkHeader );
    SHOW_ERROR_AND_RETURN;

    lArkHeader.BuildArk( CSettings::mInputDirectory.c_str() );
    lArkHeader.SaveArk( CSettings::mOutputDirectory.c_str(), CSettings::mpOutputFilename );

    return eError_NoError;
}

void PrintUsage()
{
    std::cout << "Usage: Modulate.exe <options> <command>\n\n";
    std::cout << "Options:\n";
    std::cout << "-verbose\t\tShow full information during operation\n";
    std::cout << "-force\t\t\tOverwrite existing files\n";
    std::cout << "-infile <filename>\tOpen the specified header file, rather than the default (" << CSettings::mpInputFilename << ")\n";
    std::cout << "-outfile <filename>\tSave to the specified header file, rather than the default (" << CSettings::mpOutputFilename << ")\n";
    std::cout << "-indir <pathname>\tRead data to pack from this path, rather than the working directory\n\n";
    std::cout << "-outdir <pathname>\tStore saved files in this path, rather than the working directory\n\n";
    std::cout << "Commands:\n";
    std::cout << "-unpack\t\t\tUnpack the contents of the .ark file(s)\n";
    std::cout << "-repack\t\t\tRepack data into an .ark file\n";
}

int main( int argc, char *argv[], char *envp[] )
{
    struct sCommandPair
    {
        const char* mpCommandName;
        std::function<eError( const char* )> mFunction;
        bool mbIsFinalCommand;
    };
    sCommandPair kaCommands[] = {
        "-verbose",     EnableVerbose,      false,
        "-force",       EnableForceWrite,   false,
        "-infile",      SetInputFile,       false,
        "-outfile",     SetOutputFile,      false,
        "-indir",       SetInputDirectory,  false,
        "-outdir",      SetOutputDirectory, false,
        "unpack",       Unpack,             true,
        "buildsongs",   BuildSongs,         false,
        "pack",         Pack,               true,
        nullptr,        nullptr,            false
    };

    bool lbPerformedFinalCommand = false;
    int liCommandsPerformed = 0;

    sCommandPair* lpCommand = kaCommands;
    do
    {
        for( int ii = 0; ii < argc; ++ii )
        {
            if( _stricmp( argv[ ii ], lpCommand->mpCommandName ) == 0 )
            {
                if( lbPerformedFinalCommand && lpCommand->mbIsFinalCommand )
                {
                    continue;
                }

                lbPerformedFinalCommand |= lpCommand->mbIsFinalCommand;
                ++liCommandsPerformed;

                const char* lpNextArg = ( ii < ( argc - 1 ) ) ? argv[ ii + 1 ] : "";
                eError leError = lpCommand->mFunction( lpNextArg );
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
        }

        ++lpCommand;
    } while( lpCommand->mpCommandName );

    if( liCommandsPerformed == 0 )
    {
        PrintUsage();
    }

    return 0;
}

int main2( int argc, char *argv[], char *envp[] )
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
