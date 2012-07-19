#include "aicomponent.h"
#include "worldmap.h"
#include "gamelimits.h"
#include "pathmovecomponent.h"
#include "gameitem.h"

#include "../script/battlemechanics.h"

#include "../engine/engine.h"
#include "../engine/particle.h"

#include "../xegame/chitbag.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/itemcomponent.h"

#include "../grinliz/glrectangle.h"
#include <climits>

using namespace grinliz;

static const U32	UPDATE_COMBAT_INFO	= 1000;		// how often to update the friend/enemy lists
static const float	COMBAT_INFO_RANGE	= 10.0f;	// range around to scan for friendlies/enemies


AIComponent::AIComponent( Engine* _engine, WorldMap* _map )
{
	engine = _engine;
	map = _map;
	currentAction = 0;
}


AIComponent::~AIComponent()
{
}


int AIComponent::GetTeamStatus( Chit* other )
{
	// FIXME: placeholder friend/enemy logic
	ItemComponent* thisItem  = GET_COMPONENT( parentChit, ItemComponent );
	ItemComponent* otherItem = GET_COMPONENT( other, ItemComponent );
	if ( thisItem && otherItem ) {
		if ( thisItem->GetItem()->ToGameItem()->primaryTeam != otherItem->GetItem()->ToGameItem()->primaryTeam ) {
			return ENEMY;
		}
	}
	return FRIENDLY;
}


/*
void AIComponent::UpdateChitData()
{
	for( int k=0; k<2; ++k ) {
		CArray<ChitData, MAX_TRACK>& list = (k==0) ? friendList : enemyList;

		int i=0; 
		while( i < list.Size() ) {
			Chit* chit = GetChitBag()->GetChit( list[i].chitID );
			list[i].chit = chit;
			if ( chit && chit->GetSpatialComponent() ) {
				list[i].range = ( parentChit->GetSpatialComponent()->GetPosition() - chit->GetSpatialComponent()->GetPosition() ).Length();
				++i;
			}
			else {
				list.SwapRemove( i );
			}
		}
	}
}
*/


void AIComponent::UpdateCombatInfo( const Rectangle2F* _zone )
{
	SpatialComponent* sc = parentChit->GetSpatialComponent();
	if ( !sc ) return;
	Vector2F center = sc->GetPosition2D();

	Rectangle2F zone;
	if ( !_zone ) {
		// Generate the default zone.
		zone.min = center; zone.max = center;
		zone.Outset( COMBAT_INFO_RANGE );
	}
	else {
		// Use the passed in info, add to the existing.
		zone = *_zone;
	}
	// Clear and reset the existing info.
	friendList.Clear();
	enemyList.Clear();

	// Sort in by as-the-crow-flies range. Not correct, but don't want to deal with arbitrarily long query.
	const CDynArray<Chit*>& chitArr = GetChitBag()->QuerySpatialHash( zone, parentChit, true );

	for( int i=0; i<chitArr.Size(); ++i ) {
		Chit* chit = chitArr[i];

		int teamStatus = GetTeamStatus( chit );

		if ( teamStatus == FRIENDLY && friendList.HasCap() )
			friendList.Push( chit->ID() );
		else if ( teamStatus == ENEMY && enemyList.HasCap() )
			 enemyList.Push( chit->ID() );
	}
}


void AIComponent::DoSlowTick()
{
	UpdateCombatInfo();
}


void AIComponent::DoMelee()
{
	// Are we close enough to hit? Then swing. Else move to target.

}


void AIComponent::DoTick( U32 deltaTime )
{
	// Are we doing something? Then do that; if not, look for
	// something else to do.
	if ( currentAction ) {

		if ( parentChit->GetRenderComponent() && parentChit->GetRenderComponent()->AnimationBusy() ) {
			// just wait.
		}
		else {
			switch( currentAction ) {

			case MELEE:
				DoMelee();
				break;

			default:
				GLASSERT( 0 );
				currentAction = 0;
			}
		}
	}
	else {
		
		if ( !enemyList.Empty() ) {
			currentAction = MELEE;
			action.melee.targetID = enemyList[0];
		}

	}
	/*
	// Check for events that change the situation
	const CDynArray<ChitEvent>& events = GetChitBag()->GetEvents();
	ItemComponent* itemComp = GET_COMPONENT( parentChit, ItemComponent );

	for( int i=0; i<events.Size(); ++i ) {
		if(    events[i].id == AI_EVENT_AWARENESS 
			&& itemComp
			&& events[i].data0 == itemComp->GetItem()->ToGameItem()->primaryTeam ) 
		{
			// FIXME: double call can cause 2 entries for the same unit
			UpdateCombatInfo( &events[i].bounds );
		}
	}

	if ( enemyList.Size() > 0 ) {
		const ChitData& target = enemyList[0];

		SpatialComponent*  thisSpatial = parentChit->GetSpatialComponent();
		RenderComponent*   thisRender = parentChit->GetRenderComponent();
		SpatialComponent*  targetSpatial = target.chit->GetSpatialComponent();
		PathMoveComponent* pmc = GET_COMPONENT( parentChit, PathMoveComponent );
		GLASSERT( pmc );
		if ( !pmc ) return;

		U32 absTime = GetChitBag()->AbsTime();

		grinliz::CArray<XEItem*, MAX_ACTIVE_ITEMS> activeItems;
		GameItem::GetActiveItems( parentChit, &activeItems );
		// FIXME: choose weapons, etc.
		XEItem* xeitem = activeItems[0];
		GameItem* gameItem = xeitem->ToGameItem();
		WeaponItem* weapon = gameItem->ToWeapon();

		GLASSERT( pmc );
		GLASSERT( thisSpatial );
		GLASSERT( targetSpatial );
		GLASSERT( thisRender );

		static const Vector3F UP = { 0, 1, 0 };
		const Vector3F* eyeDir = engine->camera.EyeDir3();

		if ( activeItems.Size() > 0 ) {
			// fixme: use best item for situation

			if (    BattleMechanics::InMeleeZone( thisSpatial->GetPosition2D(),
												  thisSpatial->GetHeading2D(),
												  targetSpatial->GetPosition2D() ))
			{
				if ( weapon->CanMelee( absTime ) )
				{
					BattleMechanics::MeleeAttack( engine, parentChit, weapon );																	
				}
			}
			else {
				Vector2F delta = enemyList[0].chit->GetSpatialComponent()->GetPosition2D() - thisSpatial->GetPosition2D();
				float targetRot = NormalizeAngleDegrees( ToDegree( atan2f( delta.x, delta.y )));

				pmc->QueueDest( enemyList[0].chit->GetSpatialComponent()->GetPosition2D(),
								targetRot );
			}
		}
	}
	*/
}


void AIComponent::DebugStr( grinliz::GLString* str )
{
	str->Format( "[AI] " );
}


void AIComponent::OnChitMsg( Chit* chit, int id, const ChitEvent* event )
{
}
