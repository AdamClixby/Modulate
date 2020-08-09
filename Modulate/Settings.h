#pragma once

#include <string>

class CSettings
{
public:
    static bool mbPS4;
    static const char* msPlatform;

    static bool mbVerbose;
    static bool mbOverwriteOutputFiles;
    static bool mbIgnoreNewFiles;

    static const unsigned int kuEncryptedVersionPS3 = 0xc64eed30;
    static const unsigned int kuEncryptedVersionPS4 = 0x6f303f55;

    static const unsigned int kuEncryptedPS3Key = 0xc64eed30;
    static const unsigned int kuEncryptedPS4Key = 0x90cfc0ab;
};

#define VERBOSE_OUT( out ) if( CSettings::mbVerbose ) std::cout << out 
