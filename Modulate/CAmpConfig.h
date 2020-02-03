#pragma once

#include <string>

class CAmpConfig
{
    struct sUnlock
    {
        int mi1 = 0;
        short ms1 = 0;
        short msIndex = 0;
        int mi2 = 0;
        std::string mCondition;
        int mi3 = 0;
        int mi4 = 0;
        int mi5 = 0;
        std::string mUnlockType;
        int mi6 = 0;
        std::string mUnlockTarget;
    };
    
    static void ReadUnlockDescription( const unsigned char*& lpStream, int liMaxToRead );
    static const char* GetTypeName( short lsType );
    static sUnlock ReadUnlock( const unsigned char*& lpStream, int liMaxToRead );
        
public:
    CAmpConfig();
    ~CAmpConfig();

    void Load( const char* lpPath );
};

