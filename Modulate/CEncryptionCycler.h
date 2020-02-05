#pragma once

class CEncryptionCycler
{
public:
    void Cycle( unsigned char* lpData, unsigned int liDataSize, int liInitialKey );

private:
    int CycleKey( int liKey ) const;
};

