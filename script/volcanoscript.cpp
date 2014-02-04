#include "volcanoscript.h"

#include "../engine/serialize.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/chitbag.h"

#include "../game/worldmap.h"
#include "../game/worldinfo.h"
#include "../game/reservebank.h"
#include "../game/lumoschitbag.h"

#include "../xarchive/glstreamer.h"

using namespace grinliz;
using namespace tinyxml2;

static const U32 SPREAD_RATE = 4000;

VolcanoScript::VolcanoScript( int p_size )
{
	maxSize = p_size;
	size = 0;
}


void VolcanoScript::Init()
{
	SpatialComponent* sc = scriptContext->chit->GetSpatialComponent();
	const ChitContext* context = scriptContext->chitBag->GetContext();

	GLASSERT( sc );
	if ( sc ) {
		Vector2F pos = sc->GetPosition2D();
		//GLOUTPUT(( "VolcanoScript::Init. pos=%d,%d\n", (int)pos.x, (int)pos.y ));
		context->worldMap->SetMagma( (int)pos.x, (int)pos.y, true );

		NewsEvent event( NewsEvent::VOLCANO, pos );
		NewsHistory::Instance()->Add( event );
	}
}


void VolcanoScript::Serialize( XStream* xs )
{
	XarcOpen( xs, "VolcanoScript" );
	XARC_SER( xs, size );
	XARC_SER( xs, maxSize );
	XarcClose( xs );
}


int VolcanoScript::DoTick( U32 delta )
{
	const ChitContext* context = scriptContext->chitBag->GetContext();
	SpatialComponent* sc = scriptContext->chit->GetSpatialComponent();
	WorldMap* worldMap = context->worldMap;

	Vector2I pos = { 0,  0 };
	GLASSERT( sc );
	if ( sc ) {
		Vector2F posF = sc->GetPosition2D();
		pos.Set( (int)posF.x, (int)posF.y );
	}
	else {
		scriptContext->chit->QueueDelete();
	}

	Rectangle2I b = worldMap->Bounds();
	int rad = scriptContext->time / SPREAD_RATE;
	if ( rad > size ) {
		// Cool (and set) the inner rectangle, make the new rectangle magma.
		// The origin stays magma until we're done.
		size = Min( rad-1, maxSize );
		
		Rectangle2I r;
		r.min = r.max = pos;
		r.Outset( size );

		for( int y=r.min.y; y<=r.max.y; ++y ) {
			for( int x=r.min.x; x<=r.max.x; ++x ) {
				if ( b.Contains( x, y ) ) {
					worldMap->SetMagma( x, y, false );
					const WorldGrid& g = worldMap->GetWorldGrid( x, y );
					// Does lots of error checking. Can set without checks:
					worldMap->SetRock( x, y, g.NominalRockHeight(), false, 0 );
				}
			}
		}

		size = rad;

		if ( rad < maxSize ) {
			worldMap->SetMagma( pos.x, pos.y, true );
			for( int y=r.min.y; y<=r.max.y; ++y ) {
				for( int x=r.min.x; x<=r.max.x; ++x ) {
					if ( b.Contains( x, y )) {
						if ( y == r.min.y || y == r.max.y || x == r.min.x || x == r.max.x ) {
							worldMap->SetMagma( x, y, true );
						}
					}
				}
			}
		}
		else {
			// Distribute gold and crystal.
			Vector2I sector = { pos.x/SECTOR_SIZE, pos.y/SECTOR_SIZE };
			const SectorData& sd = worldMap->GetWorldInfo().GetSector( sector );
			if ( sd.ports ) {
				for( int i=0; i<4; ++i ) {
					int x = r.min.x + scriptContext->chit->random.Rand( r.Width() );
					int y = r.min.y + scriptContext->chit->random.Rand( r.Height() );
					if (    worldMap->GetWorldGrid( x, y ).RockHeight()
						 || worldMap->GetWorldGrid( x, y ).Pool()) 
					{
						int gold = ReserveBank::Instance()->WithdrawVolcanoGold();
						Vector3F v3 = { (float)x+0.5f, 0, (float)y+0.5f };
						scriptContext->chitBag->NewGoldChit( v3, gold );
					}
				}
				if (    worldMap->GetWorldGrid( pos.x, pos.y ).RockHeight()
					 || worldMap->GetWorldGrid( pos.x, pos.y ).Pool()) 
				{	
					int gold = 0;
					int crystal = NO_CRYSTAL;
					Vector3F v3 = { (float)pos.x+0.5f, 0, (float)pos.y+0.5f };
					Wallet wallet = ReserveBank::Instance()->WithdrawVolcano();
					scriptContext->chitBag->NewWalletChits( v3, wallet );
				}
			}
			scriptContext->chit->QueueDelete();
		}
	}
	// Only need to be called back as often as it spreads,
	// but give a little more resolution for loading, etc.
	return SPREAD_RATE / 2 + scriptContext->chit->random.Rand( SPREAD_RATE / 4 );
}


