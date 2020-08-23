#include "pch.h"
#include "CDtaFile.h"

#include <functional>
#include <algorithm>

#include "Utils.h"

#define VALIDATE_STRING( lpString ) if( !lpString ) { delete[] lpInputData; return eError_InvalidData; }

CDtaNodeBase* CDtaNodeBase::FindNode( const std::string& lName ) const
{
    for( auto& lpChild : maChildren )
    {
        if( !lpChild->IsBaseNode() )
        {
            CDtaNode< std::string >* lpStringNode = dynamic_cast<CDtaNode< std::string >*>( lpChild );
            if( lpStringNode && lpStringNode->IsEqual( lName ) )
            {
                return lpStringNode;
            }

            continue;
        }

        CDtaNodeBase* lpFoundInChild = lpChild->FindNode( lName );
        if( lpFoundInChild )
        {
            return lpFoundInChild;
        }
    }

    return nullptr;
}

eError CDtaFile::Load( const char* lpFilename )
{
    FILE* lpInputFile = nullptr;
    fopen_s( &lpInputFile, lpFilename, "rb" );
    if( !lpInputFile )
    {
        eError leError = eError_FailedToOpenFile;
        SHOW_ERROR_AND_RETURN;
    }

    fseek( lpInputFile, 0, SEEK_END );
    int liFileSize = ftell( lpInputFile );
    fseek( lpInputFile, 0, SEEK_SET );

    char* lpInputData = new char[ liFileSize ];
    fread( lpInputData, liFileSize, 1, lpInputFile );
    fclose( lpInputFile );

    const char* lpDataPtr = lpInputData + 5;

    int liType = ENodeType_Tree1;

    while( lpDataPtr < lpInputData + liFileSize )
    {
        if( liType != ENodeType_Tree1 &&
            liType != ENodeType_Tree2 )
        {
            eError leError = eError_InvalidData;
            SHOW_ERROR_AND_RETURN_W( delete[] lpInputData );
        }

        eError leError = AddTreeNode( &mRootNode, lpDataPtr, (eNodeType)liType );
        if( leError != eError_NoError )
        {
            SHOW_ERROR_AND_RETURN_W( delete[] lpInputData );
        }

        liType = *(int*)lpDataPtr;
        lpDataPtr += sizeof( int ) * 2;
    }

    delete[] lpInputData;
    return eError_NoError;
}

std::vector< SSongConfig > CDtaFile::GetSongs() const
{
    std::vector< SSongConfig > laSongs;

    {
        CDtaNodeBase* lpNode = mRootNode.FindNode( "unlock_tokens" );
        CDtaNodeBase* lpSongListNode = lpNode->GetParent();

        enum eSongConfig
        {
            ESongConfig_Id,
            ESongConfig_Name,
            ESongConfig_UnlockTitle,
            ESongConfig_UnlockDesc,
            ESongConfig_Icon,
            ESongConfig_Type,
            ESongConfig_NumEntries
        };

        int liNumEntries = (int)lpSongListNode->GetChildren().size();
        for( CDtaNodeBase* lpSongNode : lpSongListNode->GetChildren() )
        {
            if( lpSongNode->GetChildren().size() != ESongConfig_NumEntries )
            {
                continue;
            }

            CDtaNode<std::string>* lpIdNode = dynamic_cast<CDtaNode<std::string>*>( lpSongNode->GetChildren()[ ESongConfig_Id ] );
            CDtaNode<std::string>* lpNameNode = dynamic_cast<CDtaNode<std::string>*>( lpSongNode->GetChildren()[ ESongConfig_Name ] );
            if( !lpIdNode || !lpNameNode )
            {
                continue;
            }

            bool lbIsSong = true;
            std::string lId = lpIdNode->GetValue();
            std::for_each( lId.begin(), lId.end(), [&lbIsSong]( char c )
            {
                if( c != ::toupper( c ) )
                {
                    lbIsSong = false;
                }
            } );

            if( !lbIsSong )
            {
                continue;
            }

            SSongConfig lConfig;
            lConfig.mId = lpIdNode->GetValue();
            lConfig.mName = lpNameNode->GetValue();

            laSongs.push_back( lConfig );
        }
    }



    return laSongs;
}

eError CDtaFile::Save( const char* lpFilename ) const
{
    unsigned char* lpOutputData = new unsigned char[ 1024 * 256 ];
    unsigned char* lpOutputPtr = lpOutputData;

    *lpOutputPtr++ = 1;
    *(int*)lpOutputPtr = 1;
    lpOutputPtr += sizeof( int );

    for( CDtaNodeBase* lpNode : mRootNode.GetChildren() )
    {
        lpNode->SaveToStream( lpOutputPtr );
    }

    FILE* lpOutputFile = nullptr;
    fopen_s( &lpOutputFile, lpFilename, "wb" );

    if( !lpOutputFile )
    {
        eError leError = eError_FailedToCreateFile;
        SHOW_ERROR_AND_RETURN_W( delete[] lpOutputData );
    }

    fwrite( lpOutputData, 1, lpOutputPtr - lpOutputData, lpOutputFile );
    fclose( lpOutputFile );

    delete[] lpOutputData;
    return eError_NoError;
}

eError CDtaFile::AddTreeNode( CDtaNodeBase* lpParent, const char*& lpData, eNodeType leNodeType )
{
    int liNumChildren = *(short*)lpData;
    lpData += sizeof( short );

    if( liNumChildren <= 0 )
    {
        return eError_InvalidData;
    }

    short liNodeId = *(short*)lpData;
    lpData += sizeof( short );

    CDtaNodeBase* lpNode = nullptr;
    if( !lpParent )
    {
        lpNode = &mRootNode;
        lpNode->SetId( liNodeId );
    }
    else
    {
        lpNode = new CDtaNodeBase( lpParent, liNodeId );
        lpParent->AddChild( lpNode );
    }
    lpNode->SetTypeOverride( leNodeType );

    for( int ii = 0; ii < liNumChildren; ++ii )
    {
        int liType = *(int*)lpData;
        lpData += sizeof( int );

        switch( (eNodeType)liType )
        {
        case ENodeType_String:
        case ENodeType_Filename:
        case ENodeType_Define:
        case ENodeType_Id:
        {
            int liStringLength = *(int*)lpData;
            lpData += sizeof( int );
            
            if( liStringLength < 0 )
            {
                return eError_InvalidData;
            }

            if( liStringLength == 0 )
            {
                if( CDtaNodeBase* lpNewNode = lpNode->AddChild( std::string( "" ) ) )
                {
                    lpNewNode->SetTypeOverride( liType );
                }
            }
            else
            {
                char* lpNewString = new char[ liStringLength + 1 ];
                memcpy( lpNewString, lpData, liStringLength );
                lpNewString[ liStringLength ] = 0;

                if( CDtaNodeBase* lpNewNode = lpNode->AddChild( std::string( lpNewString ) ) )
                {
                    lpNewNode->SetTypeOverride( liType );
                }

                delete[] lpNewString;
            }

            lpData += liStringLength;
        }
        break;

        case ENodeType_Tree2:
        case ENodeType_Tree1:
        {
            lpData += sizeof( int );
            eError leError = AddTreeNode( lpNode, lpData, (eNodeType)liType );
            if( leError != eError_NoError )
            {
                return leError;
            }
        }
        break;

        case ENodeType_Integer0:
        case ENodeType_Integer6:
        case ENodeType_Integer8:
        case ENodeType_Integer9:
        {
            int liValue = *(int*)lpData;
            lpData += sizeof( int );

            if( CDtaNodeBase* lpNewNode = lpNode->AddChild( liValue ) )
            {
                lpNewNode->SetTypeOverride( liType );
            }
        }
        break;

        case ENodeType_Float:
        {
            float lfValue = *(float*)lpData;
            lpData += sizeof( float );

            if( CDtaNodeBase* lpNewNode = lpNode->AddChild( lfValue ) )
            {
                lpNewNode->SetTypeOverride( liType );
            }
        }
        break;

        default:
            return eError_InvalidData;
        };
    }

    return eError_NoError;
}

eError CMoggsong::LoadMoggSong( const char* lpFilename )
{
    FILE* lpInputFile = nullptr;
    fopen_s( &lpInputFile, lpFilename, "rb" );
    if( !lpInputFile )
    {
        eError leError = eError_FailedToOpenFile;
        SHOW_ERROR_AND_RETURN;
    }

    fseek( lpInputFile, 0, SEEK_END );
    int liFileSize = ftell( lpInputFile );
    fseek( lpInputFile, 0, SEEK_SET );

    char* lpInputData = new char[ liFileSize ];
    fread( lpInputData, liFileSize, 1, lpInputFile );
    fclose( lpInputFile );

    char* lpDataPtr = lpInputData;

    do {
        lpDataPtr = strstr( lpDataPtr, "(" );
        VALIDATE_STRING( lpDataPtr );

        ++lpDataPtr;
        char* lpKeyEnd = strstr( lpDataPtr, " " );
        if( !lpKeyEnd )
        {
            lpKeyEnd = lpDataPtr + strlen( lpDataPtr );
        }

        *lpKeyEnd = 0;
        eError leError = ProcessMoggSongKey( lpDataPtr );
        SHOW_ERROR_AND_RETURN_W( delete[] lpInputData );
    } while( lpDataPtr - lpInputData < liFileSize - 10 );

    delete[] lpInputData;

    return eError_NoError;
}

eError CMoggsong::Save( const char* lpFilename ) const
{
    FILE* lpOutputFile = nullptr;
    fopen_s( &lpOutputFile, lpFilename, "rb" );
    if( lpOutputFile )
    {
        if( !CSettings::mbOverwriteOutputFiles )
        {
            fseek( lpOutputFile, 0, SEEK_END );
            int liFileSize = ftell( lpOutputFile );
            if( liFileSize != 0 )
            {
                VERBOSE_OUT( "Output file already exists: " << lpFilename << "\n" );
                return eError_FailedToCreateFile;
            }
        }

        fclose( lpOutputFile );
    }

    const int kiMaxDataSize = 1024 * 512;
    unsigned char lacData[ kiMaxDataSize ];
    
    unsigned char* lpDataPtr = lacData;

    CDtaNodeBase lRoot( nullptr, 1 );

    const short ksMoggPathNodeId = 1;
    CDtaNodeBase* lpMoggPathNode = lRoot.AddNode( ksMoggPathNodeId );
    {
        lpMoggPathNode->AddChild( std::string( "mogg_path") );
        lpMoggPathNode->AddChild( mMoggPath );
    }

    const short ksMidiPathNodeId = 2;
    CDtaNodeBase* lpMidiPathNode = lRoot.AddNode( ksMidiPathNodeId );
    {
        lpMidiPathNode->AddChild( std::string( "midi_path" ) );
        lpMidiPathNode->AddChild( mMidiPath );
    }

    const short ksSongInfoNodeId = 3;
    CDtaNodeBase* lpSongInfoNode = lRoot.AddNode( ksSongInfoNodeId );
    {
        lpSongInfoNode->AddChild( std::string( "song_info" ) );

        const short ksLengthNodeId = 4;
        CDtaNodeBase* lpLengthNode = lpSongInfoNode->AddNode( ksLengthNodeId );
        {
            lpLengthNode->AddChild( std::string( "length" ) );

            char lacLengthString[ 8 ];
            _itoa_s( miLength, lacLengthString, 10 );
            std::string lLengthString = lacLengthString;
            lLengthString += ":0:0";
            lpLengthNode->AddChild( lLengthString );
        }

        const short ksCountinNodeId = 5;
        CDtaNodeBase* lpCountInNode = lpSongInfoNode->AddNode( ksCountinNodeId );
        {
            lpCountInNode->AddChild( std::string( "countin" ) );
            lpCountInNode->AddChild( miCountIn );
        }
    }

    const short ksTracksNodeId = 8;
    CDtaNodeBase* lpTracksNode = lRoot.AddNode( ksTracksNodeId );
    {
        lpTracksNode->AddChild( std::string( "tracks" ) );
        {
            int liTrackIndex = 0;
            CDtaNodeBase* lpTrackArrayNode = lpTracksNode->AddNode( ksTracksNodeId + 1 );
            for( std::vector< sTrackDefinition >::const_iterator lIter = maTracks.begin(); lIter != maTracks.end(); ++lIter, ++liTrackIndex )
            {
                const sTrackDefinition& lTrack = *lIter;
                CDtaNodeBase* lpTrackNode = lpTrackArrayNode->AddNode( ksTracksNodeId + 2 + liTrackIndex );
                {
                    lpTrackNode->AddChild( lTrack.mName );
                    CDtaNodeBase* lpChannelsNode = lpTrackNode->AddNode( ksTracksNodeId + 2 + liTrackIndex );
                    {
                        for( std::vector< int >::const_iterator lChannelIter = lTrack.maChannels.begin(); lChannelIter != lTrack.maChannels.end(); ++lChannelIter )
                        {
                            lpChannelsNode->AddChild( *lChannelIter );
                        }
                    }
                }
                lpTrackNode->AddChild( lTrack.mEventString );   
            }
        }
    }

    const short ksPansNodeId = 24;
    CDtaNodeBase* lpPansNode = lRoot.AddNode( ksPansNodeId );
    {
        lpPansNode->AddChild( std::string( "pans" ) );
        CDtaNodeBase* lpPanArrayNode = lpPansNode->AddNode( ksPansNodeId );
        for( std::vector< sTrackDefinition >::const_iterator lIter = maTracks.begin(); lIter != maTracks.end(); ++lIter )
        {
            const sTrackDefinition& lTrack = *lIter;
            for( std::vector< float >::const_iterator lPanIter = lTrack.maPans.begin(); lPanIter != lTrack.maPans.end(); ++lPanIter )
            {
                lpPanArrayNode->AddChild( *lPanIter );
            }
        }
    }

    const short ksVolsNodeId = 25;
    CDtaNodeBase* lpVolsNode = lRoot.AddNode( ksPansNodeId );
    {
        lpVolsNode->AddChild( std::string( "vols" ) );
        CDtaNodeBase* lpVolArrayNode = lpVolsNode->AddNode( ksPansNodeId );
        for( std::vector< sTrackDefinition >::const_iterator lIter = maTracks.begin(); lIter != maTracks.end(); ++lIter )
        {
            const sTrackDefinition& lTrack = *lIter;
            for( std::vector< float >::const_iterator lVolIter = lTrack.maVolumes.begin(); lVolIter != lTrack.maVolumes.end(); ++lVolIter )
            {
                lpVolArrayNode->AddChild( *lVolIter );
            }
        }
    }

    const short ksTracksDBNodeId = 28;
    CDtaNodeBase* lpTrackDBNode = lRoot.AddNode( ksTracksDBNodeId );
    {
        lpTrackDBNode->AddChild( std::string( "active_track_db" ) );
        for( int ii = 0; ii < 7; ++ii )
        {
            lpTrackDBNode->AddChild( 0.0f );
        }
    }

    const short ksArenaPathNodeId = 31;
    CDtaNodeBase* lpArenaPathNode = lRoot.AddNode( ksArenaPathNodeId );
    {
        lpArenaPathNode->AddChild( std::string( "arena_path" ) );
        lpArenaPathNode->AddChild( mArenaPath );
    }

    const short ksScoreGoalNodeId = 32;
    CDtaNodeBase* lpScoreGoalNode = lRoot.AddNode( ksScoreGoalNodeId );
    {
        lpScoreGoalNode->AddChild( std::string( "score_goal" ) );
        for( int ii = 0; ii < eDifficulty_NumTypes; ++ii )
        {
            CDtaNodeBase* lpDifficultyNode = lpScoreGoalNode->AddNode( ksScoreGoalNodeId + ii + 1 );
            {
                for( int jj = 0; jj < eStars_NumTypes; ++jj )
                {
                    lpDifficultyNode->AddChild( maScoreGoals[ ii ][ jj ] );
                }
            }
        }
    }

    const short ksTunnelScaleNodeId = 40;
    CDtaNodeBase* lpTunnelScaleNode = lRoot.AddNode( ksTunnelScaleNodeId );
    {
        lpTunnelScaleNode->AddChild( std::string( "tunnel_scale" ) );
        lpTunnelScaleNode->AddChild( mfTunnelScale );
    }

    const short ksEnableOrderNodeId = 42;
    CDtaNodeBase* lpEnableOrderNode = lRoot.AddNode( ksEnableOrderNodeId );
    {
        lpEnableOrderNode->AddChild( std::string( "enable_order" ) );
        CDtaNodeBase* lpEnableOrderArrayNode = lpEnableOrderNode->AddNode( ksEnableOrderNodeId );
        {
            lpEnableOrderArrayNode->AddChild( 1 );
            lpEnableOrderArrayNode->AddChild( 2 );
            lpEnableOrderArrayNode->AddChild( 3 );
            lpEnableOrderArrayNode->AddChild( 4 );
            lpEnableOrderArrayNode->AddChild( 5 );
            lpEnableOrderArrayNode->AddChild( 6 );
        }
    }

    const short ksStartSectionBarsNodeId = 45;
    CDtaNodeBase* lpStartSectionBarsNode = lRoot.AddNode( ksStartSectionBarsNodeId );
    {
        lpStartSectionBarsNode->AddChild( std::string( "section_start_bars" ) );
        for( std::vector< int >::const_iterator lIter = maSectionStartBars.begin(); lIter != maSectionStartBars.end(); ++lIter )
        {
            lpStartSectionBarsNode->AddChild( *lIter );
        }
    }

    const int kiLocalisedStringType = 18;
    const short ksTitleNodeId = 50;
    CDtaNodeBase* lpTitleNode = lRoot.AddNode( ksTitleNodeId );
    {
        lpTitleNode->AddChild( std::string( "title" ) );
        lpTitleNode->AddChild( mTitle )->SetTypeOverride( kiLocalisedStringType );
    }

    CDtaNodeBase* lpShortTitleNode = lRoot.AddNode( ksTitleNodeId + 1 );
    {
        lpShortTitleNode->AddChild( std::string( "title_short" ) );
        lpShortTitleNode->AddChild( mTitleShort.empty() ? mTitle : mTitleShort )->SetTypeOverride( kiLocalisedStringType );
    }

    CDtaNodeBase* lpArtistNode = lRoot.AddNode( ksTitleNodeId + 2 );
    {
        lpArtistNode->AddChild( std::string( "artist" ) );
        lpArtistNode->AddChild( mArtist )->SetTypeOverride( kiLocalisedStringType );
    }

    CDtaNodeBase* lpShortArtistNode = lRoot.AddNode( ksTitleNodeId + 3 );
    {
        lpShortArtistNode->AddChild( std::string( "artist_short" ) );
        lpShortArtistNode->AddChild( mArtistShort.empty() ? mArtist : mArtistShort )->SetTypeOverride( kiLocalisedStringType );
    }

    CDtaNodeBase* lpDescNode = lRoot.AddNode( ksTitleNodeId + 4 );
    {
        lpDescNode->AddChild( std::string( "desc" ) );
        lpDescNode->AddChild( mDescription );
    }

    if( !mUnlockRequirement.empty() )
    {
        CDtaNodeBase* lpUnlockNode = lRoot.AddNode( ksTitleNodeId + 5 );
        {
            lpUnlockNode->AddChild( std::string( "unlock_requirement" ) );
            lpUnlockNode->AddChild( mUnlockRequirement );
        }
    }

    CDtaNodeBase* lpBPMNode = lRoot.AddNode( ksTitleNodeId + 6 );
    {
        lpBPMNode->AddChild( std::string( "bpm" ) );
        lpBPMNode->AddChild( (int)roundf( mfBPM ) );
    }

    if( miBossLevel >= 0 )
    {
        CDtaNodeBase* lpBossLevelNode = lRoot.AddNode( ksTitleNodeId + 7 );
        {
            lpBossLevelNode->AddChild( std::string( "boss_level" ) );
            lpBossLevelNode->AddChild( miBossLevel );
        }
    }

    CDtaNodeBase* lpPreviewStartMSNode = lRoot.AddNode( ksTitleNodeId + 8 );
    {
        lpPreviewStartMSNode->AddChild( std::string( "preview_start_ms" ) );
        lpPreviewStartMSNode->AddChild( miPreviewStartMS );
    }

    CDtaNodeBase* lpPreviewDurationMSNode = lRoot.AddNode( ksTitleNodeId + 9 );
    {
        lpPreviewDurationMSNode->AddChild( std::string( "preview_length_ms" ) );
        lpPreviewDurationMSNode->AddChild( miPreviewDurationMS );
    }

    lRoot.Save( lpDataPtr );

    std::cout << "Writing " << lpFilename << "\n";

    lpOutputFile = nullptr;
    fopen_s( &lpOutputFile, lpFilename, "wb" );
    if( !lpOutputFile )
    {
        return eError_FailedToCreateFile;
    }
    fwrite( lacData, lpDataPtr - lacData, 1, lpOutputFile );
    fclose( lpOutputFile );

    return eError_NoError;
}

eError CMoggsong::ProcessMoggSongKey( char*& lpData )
{
#define IS_KEY( lpPossibleKey ) ( strstr( lpData, lpPossibleKey ) == lpData )

    auto lReadString = [&]( std::string& lString )
    {
        char* lpPath = lpData + strlen( lpData ) + 1;
        char* lpPathEnd = ( *lpPath == '\"' ) ? strstr( lpPath, "\")" ) : strstr( lpPath, ")" );
        char* lpPathEndAlt = ( *lpPath == '\"' ) ? strstr( lpPath, "\" )" ) : lpPathEnd;
        if( !lpPathEnd && !lpPathEndAlt )
        {
            return eError_InvalidData;
        }

        if( !lpPathEndAlt )
        {
            lpPathEndAlt = lpPathEnd;
        }

        if( *lpPath == '\"' )
        {
            ++lpPath;
        }

        char* lpString = new char[ lpPathEndAlt - lpPath + 1 ];
        memcpy_s( lpString, lpPathEndAlt - lpPath, lpPath, lpPathEndAlt - lpPath );
        lpString[ lpPathEndAlt - lpPath ] = 0;
        lString = lpString;
        delete[] lpString;

        lpData = lpPathEndAlt;

        return eError_NoError;
    };

    if( mMoggPath.empty() && IS_KEY( "mogg_path" ) )
    {
        return lReadString( mMoggPath );
    }

    if( mMidiPath.empty() && strstr( lpData, "midi_path" ) == lpData )
    {
        return lReadString( mMidiPath );
    }

    if( strstr( lpData, "song_info" ) == lpData )
    {
        char* lpNewDataPtr;
        do
        {
            lpNewDataPtr = strstr( lpData, "(" );
            if( !lpNewDataPtr )
            {
                lpData += strlen( lpData ) + 1;
            }
        } while( !lpNewDataPtr );

        lpData = lpNewDataPtr;
        return eError_NoError;
    }

    if( miLength == 0 && strstr( lpData, "length" ) == lpData )
    {
        std::string lLengthString;
        eError leError = lReadString( lLengthString );
        ERROR_RETURN;

        const char* lpBreak = strstr( lLengthString.c_str(), ":" );
        if( !lpBreak )
        {
            return eError_InvalidData;
        }

        lLengthString = lLengthString.substr( 0, lpBreak - lLengthString.c_str() );
        miLength = atoi( lLengthString.c_str() );
        return eError_NoError;
    }

    auto lReadFloat = [ & ]( float& lValue )
    {
        std::string lCountinString;
        eError leError = lReadString( lCountinString );
        ERROR_RETURN;

        lValue = (float)atof( lCountinString.c_str() );
        return eError_NoError;
    };

    auto lReadInt = [ & ]( int& lValue)
    {
        std::string lCountinString;
        eError leError = lReadString( lCountinString );
        ERROR_RETURN;

        lValue = atoi( lCountinString.c_str() );
        return eError_NoError;
    };

    if( miCountIn == 0 && strstr( lpData, "countin" ) == lpData )
    {
        return lReadInt( miCountIn );
    }

    if( maTracks.empty() && strstr( lpData, "tracks" ) == lpData )
    {
        lpData += strlen( lpData ) + 1;

        char* lpTrackGroup = strstr( lpData, "(" );
        if( !lpTrackGroup )
        {
            return eError_InvalidData;
        }

        char* lpTrack = strstr( lpTrackGroup + 1, "(" );
        while( lpTrack )
        {
            ++lpTrack;

            char* lpTrackNameEnd = strstr( lpTrack, " " );
            if( !lpTrackNameEnd )
            {
                return eError_InvalidData;
            }
            *lpTrackNameEnd = 0;

            maTracks.push_back( sTrackDefinition() );
            maTracks.back().mName = lpTrack;

            lpTrack += strlen( lpTrack ) + 1;
            lpTrack = strstr( lpTrack, "(" );
            if( !lpTrack )
            {
                return eError_InvalidData;
            }
            ++lpTrack;

            char* lpChannelsEnd = strstr( lpTrack, ")" );
            if( !lpChannelsEnd )
            {
                return eError_InvalidData;
            }

            do {
                int liChannel = atoi( lpTrack );
                maTracks.back().maChannels.push_back( liChannel );
                lpTrack = strstr( lpTrack, " " ) + 1;
            } while( lpTrack < lpChannelsEnd );

            lpTrack = lpChannelsEnd + 1;
            char* lpEventName = strstr( lpTrack, " " );
            if( !lpEventName )
            {
                return eError_InvalidData;
            }
            ++lpEventName;

            char* lpEventNameEnd = strstr( lpEventName, ")" );
            if( !lpEventNameEnd )
            {
                return eError_InvalidData;
            }

            *lpEventNameEnd = 0;
            maTracks.back().mEventString = lpEventName;

            lpData = lpEventNameEnd + 1;

            char* lpEnd = strstr( lpData, ")" );
            lpTrack = strstr( lpData, "(" );

            if( lpEnd < lpTrack )
            {
                lpData = lpEnd + 1;
                break;
            }
        }

        return eError_NoError;
    }

    auto lSaveArrayToTracks = [&]( std::function< void( sTrackDefinition*, float ) > lSetFunc )
    {
        int liTrackIndex = 0;
        int liPanIndex = 0;
        char* lpPan = strstr( lpData + strlen( lpData ) + 1, "(" );
        ++lpPan;

        char* lpPansEnd = strstr( lpPan, ")" );
        if( !lpPansEnd )
        {
            return eError_InvalidData;
        }

        do {
            while( *lpPan == ' ' )
            {
                ++lpPan;
            }

            if( liTrackIndex >= maTracks.size() )
            {
                return eError_InvalidData;
            }

            float lfPan = (float)atof( lpPan );

            sTrackDefinition* lpTrack = &maTracks[ liTrackIndex ];
            lSetFunc( lpTrack, lfPan );
            ++liPanIndex;

            if( liPanIndex >= lpTrack->maChannels.size() )
            {
                ++liTrackIndex;
                liPanIndex = 0;
            }

            lpPan = strstr( lpPan, " " ) + 1;
        } while( lpPan < lpPansEnd );

        lpData = lpPansEnd + 1;

        return eError_NoError;
    };

    if( !maTracks.empty() && maTracks.front().maPans.empty() && strstr( lpData, "pans" ) == lpData )
    {
        return lSaveArrayToTracks( []( sTrackDefinition* lTrack, float lfValue ) { lTrack->maPans.push_back( lfValue );  } );
    }

    if( !maTracks.empty() && maTracks.front().maVolumes.empty() && strstr( lpData, "vols" ) == lpData )
    {
        return lSaveArrayToTracks( []( sTrackDefinition* lTrack, float lfValue ) { lTrack->maVolumes.push_back( lfValue );  } );
    }

    auto lSaveFloatArray = [&]( std::function< void( float ) > lStoreFunc )
    {
        char* lpTrackDB = lpData + strlen( lpData ) + 1;
        char* lpTrackDBEnd = strstr( lpTrackDB, ")" );

        do {
            float lfDB = (float)atof( lpTrackDB );
            lStoreFunc( lfDB );

            lpTrackDB = strstr( lpTrackDB, " " ) + 1;
            while( *lpTrackDB == ' ' )
            {
                ++lpTrackDB;
            }
        } while( lpTrackDB < lpTrackDBEnd );

        lpData = lpTrackDBEnd;

        return eError_NoError;
    };

    if( maActiveTrackDB.empty() && strstr( lpData, "active_track_db" ) == lpData )
    {
        return lSaveFloatArray( [ & ]( float lfValue ) { maActiveTrackDB.push_back( lfValue ); } );
    }

    if( mArenaPath.empty() && strstr( lpData, "arena_path" ) == lpData )
    {
        return lReadString( mArenaPath );
    }

    if( maScoreGoals[ 0 ][ 0 ] == 0 && strstr( lpData, "score_goal" ) == lpData )
    {
        char* lpStart = strstr( lpData + strlen( lpData ) + 1, "(" );
        char* lpEnd = strstr( lpStart, ")" );

        int liDifficulty = eDifficulty_Beginner;
        int liStars = eStars_2;
        do {
            if( liDifficulty == eDifficulty_NumTypes )
            {
                return eError_InvalidData;
            }

            do {
                if( !lpStart )
                {
                    return eError_InvalidData;
                }

                ++lpStart;
                while( *lpStart == ' ' )
                {
                    ++lpStart;
                }

                int liScore = atoi( lpStart );
                maScoreGoals[ liDifficulty ][ liStars ] = liScore;
                if( ++liStars == eStars_NumTypes )
                {
                    ++liDifficulty;
                    liStars = 0;
                }

                lpStart = strstr( lpStart, " " );
            } while( lpStart < lpEnd );

            lpData = lpEnd + 1;

            lpStart = strstr( lpData, "(" );
            lpEnd = strstr( lpData, ")" );

            if( !lpStart || !lpEnd )
            {
                return eError_InvalidData;
            }
        } while( lpEnd > lpStart );
         
        return eError_NoError;
    }

    if( mfTunnelScale == 0.0f && strstr( lpData, "tunnel_scale" ) == lpData )
    {
        std::string lScaleString;
        eError leError = lReadString( lScaleString );
        ERROR_RETURN;

        mfTunnelScale = (float)atof( lScaleString.c_str() );
        return eError_NoError;
    }

    auto lSaveIntArray = [ & ]( char* lpStream, std::function< void( int ) > lStoreFunc )
    {
        char* lpStart = lpStream;
        char* lpEnd = strstr( lpStart, ")" );

        do {
            int liVal = atoi( lpStart );
            lStoreFunc( liVal );

            lpStart = strstr( lpStart, " " ) + 1;
            while( *lpStart == ' ' )
            {
                ++lpStart;
            }
        } while( lpStart < lpEnd );

        lpData = lpEnd;

        return eError_NoError;
    };

    if( maEnableOrder.empty() && IS_KEY( "enable_order" ) )
    {
        char* lpStart = strstr( lpData + strlen( lpData ) + 1, "(" );
        if( !lpStart )
        {
            return eError_InvalidData;
        }
        ++lpStart;

        return lSaveIntArray( lpStart, [ & ]( int liValue ) { maEnableOrder.push_back( liValue ); } );
    }

    if( maSectionStartBars.empty() && strstr( lpData, "section_start_bars" ) == lpData )
    {
        char* lpStart = lpData + strlen( lpData ) + 1;
        if( !lpStart )
        {
            return eError_InvalidData;
        }
        while( *lpStart == ' ' )
        {
            ++lpStart;
        }

        return lSaveIntArray( lpStart, [ & ]( int liValue ) { maSectionStartBars.push_back( liValue ); } );
    }

#define READ_STRING( lpName, lVariable ) if( lVariable.empty() && IS_KEY( lpName ) ) { return lReadString( lVariable ); }
    
    READ_STRING( "title",              mTitle );
    READ_STRING( "title_short",        mTitleShort );
    READ_STRING( "artist",             mArtist );
    READ_STRING( "artist_short",       mArtistShort );
    READ_STRING( "desc",               mDescription );
    READ_STRING( "unlock_requirement", mUnlockRequirement );

#define READ_INT( lpName, lVariable )   if( IS_KEY( lpName ) ) { return lReadInt( lVariable );   }
#define READ_FLOAT( lpName, lVariable ) if( IS_KEY( lpName ) ) { return lReadFloat( lVariable ); }

    READ_INT( "boss_level",        miBossLevel );
    READ_INT( "preview_start_ms",  miPreviewStartMS );
    READ_INT( "preview_length_ms", miPreviewDurationMS );
    READ_FLOAT( "bpm", mfBPM );

#define IGNORE_KEY( lpName ) if( IS_KEY( lpName ) ) { std::string lIgnore; return lReadString( lIgnore ); }

    std::cout << "Unknown token in file: " << lpData << "\n";

    return eError_InvalidParameter;
}

void CDtaNodeBase::Save( unsigned char*& lpStream ) const
{
    *lpStream++ = 1;
    WriteToStream< int >( lpStream, 1 );

    SaveToStream( lpStream );
}

void CDtaNodeBase::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< short >( lpStream, (short)maChildren.size() );
    WriteToStream< short >( lpStream, msNodeId );

    for( std::vector< CDtaNodeBase* >::const_iterator lIter = maChildren.begin(); lIter != maChildren.end(); ++lIter )
    {
        const CDtaNodeBase* lpChild = *lIter;
        if( lpChild->IsBaseNode() )
        {
            WriteToStream< int >( lpStream, lpChild->miTypeOverride );
            WriteToStream< int >( lpStream, 1 );
        }

        lpChild->SaveToStream( lpStream );
    }
}
