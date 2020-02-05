#pragma once

#include <string>

class CSettings
{
public:
    static const char* mpInputFilename;
    static std::string mInputDirectory;
    static const char* mpOutputFilename;
    static std::string mOutputDirectory;
    static bool mbVerbose;
    static bool mbOverwriteOutputFiles;
};

#define VERBOSE_OUT( out ) if( CSettings::mbVerbose ) std::cout << out 
