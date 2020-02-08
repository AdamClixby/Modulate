#pragma once

#include <vector>
#include <list>
#include <string>

#include "Error.h"
#include "Utils.h"

class CDtaNodeBase
{
public:
    CDtaNodeBase() : msNodeId( siNextId++ ), mbIsBaseNode( true ) {}

    virtual ~CDtaNodeBase()
    {
        for( std::list< CDtaNodeBase* >::iterator lIter = maChildren.begin(); lIter != maChildren.end(); ++lIter )
        {
            delete *lIter;
        }
    }

    bool IsBaseNode() const
    {
        return mbIsBaseNode;
    }

    CDtaNodeBase* AddNode()
    {
        maChildren.push_back( new CDtaNodeBase() );
        return maChildren.back();
    }

    template < class T >
    void AddChild( const T& lValue );

    void Save( unsigned char*& lpStream ) const;

    virtual void SaveToStream( unsigned char*& lpStream ) const;

    static int siNextId;

private:
    std::list< CDtaNodeBase* > maChildren;
    short msNodeId;

protected:
    bool mbIsBaseNode;
};


template < class T >
class CDtaNode : public CDtaNodeBase
{
public:
    CDtaNode( const T& lValue )
        : CDtaNodeBase()
    {
        mValue = lValue;
        mbIsBaseNode = false;
    }

    virtual void SaveToStream( unsigned char*& lpStream ) const override { }

private:
    T mValue;
};

template < class T >
void CDtaNodeBase::AddChild( const T& lValue )
{
    CDtaNode< T >* lNewNode = new CDtaNode< T >( lValue );
    maChildren.push_back( lNewNode );
}

void CDtaNode< int >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, 0 );
    WriteToStream< int >( lpStream, mValue );
}

void CDtaNode< unsigned int >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, 0 );
    WriteToStream< int >( lpStream, (int)mValue );
}

void CDtaNode< float >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, 1 );
    WriteToStream< float >( lpStream, mValue );
}

void CDtaNode< std::string >::SaveToStream( unsigned char*& lpStream ) const
{
    WriteToStream< int >( lpStream, 5 );
    WriteToStream( lpStream, mValue );
}


class CDtaFile
{
public:
    eError Load( const char* lpFilename );
    eError Save( const char* lpFilename ) const;

private:
    std::string mMoggPath;
    std::string mMidiPath;

    std::string mTitle;
    std::string mTitleShort;
    std::string mArtist;
    std::string mArtistShort;
    std::string mDescription;

    float mfTunnelScale;

    int miBPM;
    int miLength;
    int miCountIn;

    unsigned int muPreviewStartMS;
    unsigned int muPreviewDurationMS;

    struct sTrackDefinition
    {
        std::string mName;
        std::vector< int > maChannels;
        std::vector< float > maPans;
        std::vector< float > maVolumes;
        std::string mEventString;
    };

    std::vector< sTrackDefinition > maTracks;
    std::vector< int > maEnableOrder;
    std::vector< int > maSectionStartBars;

    std::string mArenaPath;
    
    enum eDifficulty {
        eDifficulty_Beginner,
        eDifficulty_Intermediate,
        eDifficulty_Expert,
        eDifficulty_Super,
        eDifficulty_NumTypes
    };
    enum eStars {
        eStars_2,
        eStars_3,
        eStars_G,
        eStars_NumTypes
    };
    int maScoreGoals[ eDifficulty_NumTypes ][ eStars_NumTypes ];
};

