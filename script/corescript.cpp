#include "corescript.h"

#include "../grinliz/glvector.h"

#include "../game/census.h"
#include "../game/lumoschitbag.h"
#include "../game/mapspatialcomponent.h"
#include "../game/worldmap.h"
#include "../game/aicomponent.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"

using namespace grinliz;

CoreScript::CoreScript( WorldMap* map ) 
	: worldMap( map ), 
	  spawnTick( 10*1000 ), 
	  boundID( 0 )
{
}


CoreScript::~CoreScript()
{
}


void CoreScript::Init( const ScriptContext& ctx )
{
	spawnTick.Randomize( ctx.chit->ID() );
}


void CoreScript::Serialize( const ScriptContext& ctx, XStream* xs )
{
	XarcOpen( xs, ScriptName() );
	XARC_SER( xs, boundID );
	spawnTick.Serialize( xs, "spawn" );
	XarcClose( xs );
}


void CoreScript::OnAdd( const ScriptContext& ctx )
{
	// Cores are indestructable. They don't get removed.
}


static bool Accept( Chit* c ) 
{
	AIComponent* ai = c->GetAIComponent();
	return ai != 0;
}


Chit* CoreScript::GetAttached()
{
	Chit* c = 0;
	if ( boundID ) {
		c = scriptContext->chit->GetChitBag()->GetChit( boundID );
		if ( !c ) {
			boundID = 0;
		}
	}
	return c;
}


bool CoreScript::AttachToCore( Chit* chit )
{
	Chit* bound = GetAttached();

	ComponentSet chitset( chit, Chit::SPATIAL_BIT | Chit::RENDER_BIT | ComponentSet::IS_ALIVE );
	if ( chitset.okay && !bound ) {
		// Set the position of the bound item to the core.
		Vector3F pos = scriptContext->chit->GetSpatialComponent()->GetPosition();
		chitset.spatial->SetPosition( pos );
		boundID = chit->ID();

		// Remove the Move component so it doesn't go anywhere or block other motion. But
		// still can be hit, has spatial, etc.
		Component* c = chit->GetMoveComponent();
		chit->Remove( c );
		chit->GetChitBag()->DeferredDelete( c );

		return true;
	}
	return false;
}


int CoreScript::DoTick( const ScriptContext& ctx, U32 delta, U32 since )
{
	static const int RADIUS = 4;

	if ( spawnTick.Delta( since ) && ctx.census->ais < TYPICAL_MONSTERS ) {
		// spawn stuff.
		MapSpatialComponent* ms = GET_SUB_COMPONENT( ctx.chit, SpatialComponent, MapSpatialComponent );
		GLASSERT( ms );
		Vector2I pos = ms->MapPosition();

		Rectangle2F r;
		r.Set( (float)pos.x, (float)(pos.y), (float)(pos.x+1), (float)(pos.y+1) );
		CChitArray arr;
		ctx.chit->GetChitBag()->QuerySpatialHash( &arr, r, 0, Accept );
		if ( arr.Size() < 2 ) {
			Vector3F pf = { (float)pos.x+0.5f, 0, (float)pos.y+0.5f };
			// FIXME: proper team
			// FIXME safe downcast
			Vector2F p2 = ctx.chit->GetSpatialComponent()->GetPosition2D();

			int team = 100;
			const char* asset = "mantis";
			if ( ctx.chit->ID() & 1 ) {
				team = 101;
				asset = "redMantis";
			}
			((LumosChitBag*)(ctx.chit->GetChitBag()))->NewMonsterChit( pf, asset, team );
		}
	}
	return spawnTick.Next();
}


