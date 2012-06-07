#include "battlemechanics.h"

#include "../game/gameitem.h"
#include "../game/gamelimits.h"
#include "../game/healthcomponent.h"

#include "../xegame/chitbag.h"
#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"

#include "../grinliz/glvector.h"
#include "../grinliz/glgeometry.h"

using namespace grinliz;


bool BattleMechanics::InMeleeZone( const grinliz::Vector2F& origin, 
									const grinliz::Vector2F& dir, 
									const grinliz::Vector2F& target )
{
	GLASSERT( Equal( dir.Length(), 1.0f, 0.001f ) );

	float d2 = ( target - origin ).LengthSquared();
	if ( d2 <= (MELEE_RANGE*MELEE_RANGE) ) {
		Vector2F normal = target - origin;
		normal.Normalize();
		if ( DotProduct( dir, normal ) >= MELEE_COS_THETA ) {
			return true;
		}
	}
	return false;
}


void BattleMechanics::MeleeAttack( Chit* src, WeaponItem* weapon )
{
	ChitBag* chitBag = src->GetChitBag();
	GLASSERT( chitBag );

	U32 absTime = chitBag->AbsTime();
	GLASSERT( weapon->CanMelee( absTime ) );
	weapon->DoMelee( absTime );

	// Get origin and direction of melee attack,
	// then send messages to everyone hit.
	// FIXME:
	// Use the chitBag query, which avoids smacking
	// weapons and such (good) but may not account
	// for objects in air and such (bad...)
	// Does establish that everything in the chitbag
	// query is something that can be hit by melee/etc.
	// FIXME: may never be hitting world objects

	Vector2F srcPos = src->GetSpatialComponent()->GetPosition2D();
	Vector2F srcNormal = src->GetSpatialComponent()->GetHeading2D();

	Rectangle2F b;
	b.min = srcPos; b.max = srcPos;
	b.Outset( MELEE_RANGE + MAX_BASE_RADIUS );
	const CDynArray<Chit*>& nearChits = chitBag->QuerySpatialHash( b, src, false );

	for( int i=0; i<nearChits.Size(); ++i ) {
		Chit* target = nearChits[i];
		GLASSERT( target->GetSpatialComponent() );
		if ( InMeleeZone( srcPos, srcNormal, target->GetSpatialComponent()->GetPosition2D() )) {

			// FIXME: account for armor, shields, etc. etc.
			// FIXME: account for knockback (physics move), catching fire, etc.
			// FIXME: account for critical damage
			HealthComponent* targetHealth = GET_COMPONENT( target, HealthComponent );
			if ( targetHealth ) {
				targetHealth->DeltaHealth( -weapon->BaseDamage() );
			}
		}
	}
}