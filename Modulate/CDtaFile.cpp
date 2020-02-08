#include "pch.h"
#include "CDtaFile.h"

#include "Utils.h"

int CDtaNodeBase::siNextId = 1;

eError CDtaFile::Load( const char* lpFilename )
{
    mMoggPath = "wetware.mogg";
    mMidiPath = "wetware.mid";

    mTitle = "Wetware";
    mTitleShort = "02. Wetware";
    mArtist = "Harmonix";
    mArtistShort = "Harmonix";
    mDescription = "song_desc_wetware";

    mfTunnelScale = 1.0f;

    miBPM = 130;
    miLength = 111;
    miCountIn = 4;

    muPreviewStartMS = 18461;
    muPreviewDurationMS = 9231;

    maTracks.push_back( { "drums1",     {  0,  1 }, { -1.0f, 1.0f }, { -4.9f, -4.9f }, "event:/SONG_BUS" } );
    maTracks.push_back( { "drums2",     {  2,  3 }, { -1.0f, 1.0f }, { -3.4f, -3.4f }, "event:/SONG_BUS" } );
    maTracks.push_back( { "bass",       {  4,  5 }, { -1.0f, 1.0f }, {  0.0f,  0.0f }, "event:/SONG_BUS" } );
    maTracks.push_back( { "synth1",     {  6,  7 }, { -1.0f, 1.0f }, { -1.9f, -1.9f }, "event:/SONG_BUS" } );
    maTracks.push_back( { "synth2",     {  8,  9 }, { -1.0f, 1.0f }, { -5.0f, -5.0f }, "event:/SONG_BUS" } );
    maTracks.push_back( { "vox",        { 10, 11 }, { -1.0f, 1.0f }, {  0.0f,  0.0f }, "event:/SONG_BUS" } );
    maTracks.push_back( { "freestyle",  { 12 },     {  0.0f },       {  0.0f }       , "event:/FREESTYLE_FX_1" } );
    maTracks.push_back( { "bg_click",   { 13, 14 }, { -1.0f, 1.0f }, { -2.9f, -2.9f }, "event:/SONG_BUS" } );

    maEnableOrder = { 1, 2, 3, 4, 5, 6 };
    maSectionStartBars = { 10, 20, 30, 40, 50 };

    mArenaPath = "ConstructoP2";

    maScoreGoals[ eDifficulty_Beginner     ][ eStars_2 ] = 387;
    maScoreGoals[ eDifficulty_Beginner     ][ eStars_3 ] = 518;
    maScoreGoals[ eDifficulty_Beginner     ][ eStars_G ] = 2195;

    maScoreGoals[ eDifficulty_Intermediate ][ eStars_2 ] = 657;
    maScoreGoals[ eDifficulty_Intermediate ][ eStars_3 ] = 923;
    maScoreGoals[ eDifficulty_Intermediate ][ eStars_G ] = 2195;

    maScoreGoals[ eDifficulty_Expert       ][ eStars_2 ] = 1125;
    maScoreGoals[ eDifficulty_Expert       ][ eStars_3 ] = 1574;
    maScoreGoals[ eDifficulty_Expert       ][ eStars_G ] = 2195;

    maScoreGoals[ eDifficulty_Super        ][ eStars_2 ] = 1305;
    maScoreGoals[ eDifficulty_Super        ][ eStars_3 ] = 1745;
    maScoreGoals[ eDifficulty_Super        ][ eStars_G ] = 2195;

    return eError_NoError;
}

eError CDtaFile::Save( const char* lpFilename ) const
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

    CDtaNodeBase::siNextId = 1;

    CDtaNodeBase lRoot;
    CDtaNodeBase* lpMoggPathNode = lRoot.AddNode();
    {
        lpMoggPathNode->AddChild( std::string( "mogg_path") );
        lpMoggPathNode->AddChild( mMoggPath );
    }

    CDtaNodeBase* lpMidiPathNode = lRoot.AddNode();
    {
        lpMidiPathNode->AddChild( std::string( "midi_path" ) );
        lpMidiPathNode->AddChild( mMidiPath );
    }

    CDtaNodeBase* lpSongInfoNode = lRoot.AddNode();
    {
        lpSongInfoNode->AddChild( std::string( "song_info" ) );
        CDtaNodeBase* lpLengthNode = lpSongInfoNode->AddNode();
        {
            lpLengthNode->AddChild( std::string( "length" ) );

            char lacLengthString[ 8 ];
            _itoa_s( miLength, lacLengthString, 10 );
            std::string lLengthString = lacLengthString;
            lLengthString += ":0:0";
            lpLengthNode->AddChild( lLengthString );
        }
        CDtaNodeBase* lpCountInNode = lpSongInfoNode->AddNode();
        {
            lpCountInNode->AddChild( std::string( "countin" ) );
            lpCountInNode->AddChild( miCountIn );
        }
    }

    CDtaNodeBase* lpTracksNode = lRoot.AddNode();
    {
        lpTracksNode->AddChild( std::string( "tracks" ) );
        {
            CDtaNodeBase* lpTrackArrayNode = lpTracksNode->AddNode();
            for( std::vector< sTrackDefinition >::const_iterator lIter = maTracks.begin(); lIter != maTracks.end(); ++lIter )
            {
                const sTrackDefinition& lTrack = *lIter;
                CDtaNodeBase* lpTrackNode = lpTrackArrayNode->AddNode();
                {
                    lpTrackNode->AddChild( lTrack.mName );
                    CDtaNodeBase* lpChannelsNode = lpTrackNode->AddNode();
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

    CDtaNodeBase* lpPansNode = lRoot.AddNode();
    {
        lpPansNode->AddChild( std::string( "pans" ) );
        CDtaNodeBase* lpPanArrayNode = lpPansNode->AddNode();
        for( std::vector< sTrackDefinition >::const_iterator lIter = maTracks.begin(); lIter != maTracks.end(); ++lIter )
        {
            const sTrackDefinition& lTrack = *lIter;
            for( std::vector< float >::const_iterator lPanIter = lTrack.maPans.begin(); lPanIter != lTrack.maPans.end(); ++lPanIter )
            {
                lpPanArrayNode->AddChild( *lPanIter );
            }
        }
    }

    CDtaNodeBase* lpVolsNode = lRoot.AddNode();
    {
        lpVolsNode->AddChild( std::string( "vols" ) );
        CDtaNodeBase* lpVolArrayNode = lpVolsNode->AddNode();
        for( std::vector< sTrackDefinition >::const_iterator lIter = maTracks.begin(); lIter != maTracks.end(); ++lIter )
        {
            const sTrackDefinition& lTrack = *lIter;
            for( std::vector< float >::const_iterator lVolIter = lTrack.maVolumes.begin(); lVolIter != lTrack.maVolumes.end(); ++lVolIter )
            {
                lpVolArrayNode->AddChild( *lVolIter );
            }
        }
    }

    CDtaNodeBase* lpTrackDBNode = lRoot.AddNode();
    {
        lpTrackDBNode->AddChild( std::string( "active_track_db" ) );
        for( int ii = 0; ii < 7; ++ii )
        {
            lpTrackDBNode->AddChild( 0.0f );
        }
    }

    CDtaNodeBase* lpArenaPathNode = lRoot.AddNode();
    {
        lpArenaPathNode->AddChild( std::string( "arena_path" ) );
        lpArenaPathNode->AddChild( mArenaPath );
    }

    CDtaNodeBase* lpScoreGoalNode = lRoot.AddNode();
    {
        lpScoreGoalNode->AddChild( std::string( "score_goal" ) );
        for( int ii = 0; ii < eDifficulty_NumTypes; ++ii )
        {
            CDtaNodeBase* lpDifficultyNode = lpScoreGoalNode->AddNode();
            {
                for( int jj = 0; jj < eStars_NumTypes; ++jj )
                {
                    lpDifficultyNode->AddChild( maScoreGoals[ ii ][ jj ] );
                }
            }
        }
    }

    CDtaNodeBase* lpTunnelScaleNode = lRoot.AddNode();
    {
        lpTunnelScaleNode->AddChild( std::string( "tunnel_scale" ) );
        lpTunnelScaleNode->AddChild( mfTunnelScale );
    }

    CDtaNodeBase* lpEnableOrderNode = lRoot.AddNode();
    {
        lpEnableOrderNode->AddChild( std::string( "enable_order" ) );
        CDtaNodeBase* lpEnableOrderArrayNode = lpEnableOrderNode->AddNode();
        {
            lpEnableOrderArrayNode->AddChild( 1 );
            lpEnableOrderArrayNode->AddChild( 2 );
            lpEnableOrderArrayNode->AddChild( 3 );
            lpEnableOrderArrayNode->AddChild( 4 );
            lpEnableOrderArrayNode->AddChild( 5 );
            lpEnableOrderArrayNode->AddChild( 6 );
        }
    }

    CDtaNodeBase* lpStartSectionBarsNode = lRoot.AddNode();
    {
        lpStartSectionBarsNode->AddChild( std::string( "section_start_bars" ) );
        for( std::vector< int >::const_iterator lIter = maSectionStartBars.begin(); lIter != maSectionStartBars.end(); ++lIter )
        {
            lpStartSectionBarsNode->AddChild( *lIter );
        }
    }

    CDtaNodeBase* lpTitleNode = lRoot.AddNode();
    {
        lpTitleNode->AddChild( std::string( "title" ) );
        lpTitleNode->AddChild( mTitle );
    }

    CDtaNodeBase* lpShortTitleNode = lRoot.AddNode();
    {
        lpShortTitleNode->AddChild( std::string( "title_short" ) );
        lpShortTitleNode->AddChild( mTitleShort );
    }

    CDtaNodeBase* lpArtistNode = lRoot.AddNode();
    {
        lpArtistNode->AddChild( std::string( "artist" ) );
        lpArtistNode->AddChild( mArtist );
    }

    CDtaNodeBase* lpShortArtistNode = lRoot.AddNode();
    {
        lpShortArtistNode->AddChild( std::string( "artist_short" ) );
        lpShortArtistNode->AddChild( mArtistShort );
    }

    CDtaNodeBase* lpDescNode = lRoot.AddNode();
    {
        lpDescNode->AddChild( std::string( "desc" ) );
        lpDescNode->AddChild( mDescription );
    }

    //CDtaNodeBase* lpUnlockNode = lRoot.AddNode();
    //{
    //    lpUnlockNod->AddChild( std::string( "unlock_requirement" ) );
    //    lpUnlockNod->AddChild( mUnlockRequirement );
    //}

    CDtaNodeBase* lpBPMNode = lRoot.AddNode();
    {
        lpBPMNode->AddChild( std::string( "bpm" ) );
        lpBPMNode->AddChild( miBPM );
    }

    CDtaNodeBase* lpPreviewStartMSNode = lRoot.AddNode();
    {
        lpPreviewStartMSNode->AddChild( std::string( "preview_start_ms" ) );
        lpPreviewStartMSNode->AddChild( muPreviewStartMS );
    }

    CDtaNodeBase* lpPreviewDurationMSNode = lRoot.AddNode();
    {
        lpPreviewDurationMSNode->AddChild( std::string( "preview_length_ms" ) );
        lpPreviewDurationMSNode->AddChild( muPreviewDurationMS );
    }

    lRoot.Save( lpDataPtr );

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

    for( std::list< CDtaNodeBase* >::const_iterator lIter = maChildren.begin(); lIter != maChildren.end(); ++lIter )
    {
        const CDtaNodeBase* lpChild = *lIter;
        if( lpChild->IsBaseNode() )
        {
            WriteToStream< int >( lpStream, 16 );
            WriteToStream< int >( lpStream, 1 );
        }

        lpChild->SaveToStream( lpStream );
    }
}
