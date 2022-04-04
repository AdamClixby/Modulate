#pragma once

#include "CDtaFile.h"

void CDtaNode< int >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, miTypeOverride == -1 ? ENodeType_Integer0 : miTypeOverride );
    WriteToStream< int >( lpStream, mValue );
}

void CDtaNode< unsigned int >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, miTypeOverride == -1 ? ENodeType_Integer0 : miTypeOverride );
    WriteToStream< int >( lpStream, (int)mValue );
}

void CDtaNode< float >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, miTypeOverride == -1 ? ENodeType_Float : miTypeOverride );
    WriteToStream< float >( lpStream, mValue );
}

void CDtaNode< std::string >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, miTypeOverride == -1 ? ENodeType_String : miTypeOverride );
    WriteToStream( lpStream, mValue );
}
