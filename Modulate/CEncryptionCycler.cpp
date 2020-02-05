#include "pch.h"
#include "CEncryptionCycler.h"

void CEncryptionCycler::Cycle( unsigned char* lpData, unsigned int liDataSize, int liInitialKey )
{
    int liOffset = 0;
    int liKey = CycleKey( liInitialKey );

    for( unsigned int ii = 0; ii < liDataSize; ++ii, ++lpData )
    {
        *lpData = *lpData ^ ( liKey ^ 0xFF );
        liKey = CycleKey( liKey );
    }
}

int CEncryptionCycler::CycleKey( int liKey ) const
{
    int liResult = ( liKey - ( ( liKey / 0x1F31D ) * 0x1F31D ) ) * 0x41A7 - ( liKey / 0x1F31D ) * 0xB14;
    if( liResult <= 0 )
    {
        liResult += 0x7FFFFFFF;
    }

    return liResult;
}
