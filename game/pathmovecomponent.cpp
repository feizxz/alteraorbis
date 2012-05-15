#include "pathmovecomponent.h"
#include "worldmap.h"

#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/chitbag.h"

#include "../engine/loosequadtree.h"

#include "../grinliz/glperformance.h"

#include <cstring>
#include <cmath>

using namespace grinliz;

//#define DEBUG_PMC

void PathMoveComponent::OnAdd( Chit* chit )
{
	MoveComponent::OnAdd( chit );
	SetNoPath();
	blockForceApplied = false;
	avoidForceApplied = false;
	isStuck = false;
	queuedDest.Set( -1, -1 );
}


void PathMoveComponent::OnRemove()
{
	MoveComponent::OnRemove();
}


void PathMoveComponent::QueueDest( const grinliz::Vector2F& d )
{
	SetNoPath();
	GLASSERT(  d.x >= 0 && d.y >= 0 );
	queuedDest = d;
}


void PathMoveComponent::ComputeDest( const Vector2F& d )
{
	GRINLIZ_PERFTRACK;

	SpatialComponent* spatial = parentChit->GetSpatialComponent();
	GLASSERT( spatial );
	RenderComponent* render = parentChit->GetRenderComponent();
	GLASSERT( render );

	const Vector3F& posVec = spatial->GetPosition();

	Vector2F start = { posVec.x, posVec.z };
	dest = d;
	nPathPos = 0;
	pathPos = 0;

	// Make sure the 'dest' is actually a point we can get to.
	float radius = render->RadiusOfBase();
	if ( map->ApplyBlockEffect( d, radius, &dest ) ) {
#ifdef DEBUG_PMC
		GLOUTPUT(( "Dest adjusted. (%.1f,%.1f) -> (%.1f,%.1f)\n", d.x, d.y, dest.x, dest.y ));
#endif
	}

	bool okay = map->CalcPath( start, dest, path, &nPathPos, MAX_MOVE_PATH, pathDebugging ); 
	if ( !okay ) {
		SetNoPath();
		SendMessage( "PathMoveComponent", MSG_DESTINATION_BLOCKED );
		GLASSERT( queuedDest.x > 0 );	// DEBUGGING
	}
	else {
		GLASSERT( nPathPos > 0 );
		GLASSERT( pathPos == 0 );
		GLASSERT( dest.x >= 0 && dest.y >= 0 );
	}
	// If pos < nPathPos, then pathing happens!
}


void PathMoveComponent::GetPosRot( grinliz::Vector2F* pos, float* rot )
{
	SpatialComponent* spatial = parentChit->GetSpatialComponent();
	GLASSERT( spatial );
	const Vector3F& pos3 = spatial->GetPosition();
	pos->Set( pos3.x, pos3.z );
	*rot = spatial->GetYRotation();
}


void PathMoveComponent::SetPosRot( const grinliz::Vector2F& pos, float rot )
{
	SpatialComponent* spatial = parentChit->GetSpatialComponent();
	GLASSERT( spatial );
	spatial->SetPosition( pos.x, 0, pos.y );
	spatial->SetYRotation( rot );
}


float PathMoveComponent::GetDistToNext2( const Vector2F& current )
{
	float dx = current.x - path[pathPos].x;
	float dy = current.y - path[pathPos].y;
	return dx*dx + dy*dy;
}


void PathMoveComponent::MoveFirst( U32 delta )
{
	if ( pathPos < nPathPos ) {

		float travel = Travel( MOVE_SPEED, delta );
		Vector2F startingPos2 = pos2;

		while ( travel > 0 && pathPos < nPathPos ) {
			startingPos2 = pos2;

			float distToNext = (pos2-path[pathPos]).Length();
			if ( distToNext <= travel ) {
				// Move to the next waypoint
				travel -= distToNext;
				pos2 = path[pathPos];
				++pathPos;
			}
			else {
				pos2 = Lerp( pos2, path[pathPos], travel / distToNext );
				travel = 0;
			}
		}
		Vector2F delta = pos2 - startingPos2;
		if ( delta.LengthSquared() ) {
			rot = ToDegree( atan2f( delta.x, delta.y ) );
		}
	}
}


void PathMoveComponent::RotationFirst( U32 delta )
{
	if ( pathPos < nPathPos ) {
		float travel    = Travel( MOVE_SPEED, delta );
		float travelRot	= Travel( ROTATION_SPEED, delta );

		while ( travel > 0 && travelRot > 0 && pathPos < nPathPos ) {
			Vector2F next  = path[pathPos];
			Vector2F delta = next - pos2;
			float    dist = delta.Length();
			
			// check for very close & patch.
			static const float EPS = 0.01f;
			if ( dist <= EPS ) {
				travel -= dist;
				pos2 = next;
				++pathPos;
				continue;
			}

			float targetRot = NormalizeAngleDegrees( ToDegree( atan2f( delta.x, delta.y )));

			float deltaRot, bias;
			MinDeltaDegrees( rot, targetRot, &deltaRot, &bias );

			if ( travelRot > deltaRot ) {
				rot = targetRot;
				travelRot -= deltaRot;
			}
			else {
				rot += travelRot*bias;
				rot = NormalizeAngleDegrees( rot );
				travelRot = 0;
			}

			// We we don't chase the destination point, line
			// it up closer
			float rotationLimit = ROTATION_LIMIT;
			if ( dist < 1.0f ) {
				rotationLimit = Lerp( 0.0f, ROTATION_LIMIT, dist );
			}
			
			if ( deltaRot < rotationLimit ) {
				Vector2F norm = { sinf(ToRadian(rot)), cosf(ToRadian(rot)) };

				if ( dist <= travel ) {
					travel -= dist;
					pos2 = next;
					++pathPos;
				}
				else {
					pos2 += norm * travel;
					travel = 0;
				}
			}
		}
	}
}


bool PathMoveComponent::AvoidOthers( U32 delta )
{
	GRINLIZ_PERFTRACK;
	static const float PATH_AVOID_DISTANCE = MAX_BASE_RADIUS*2.0f;

	avoidForceApplied = false;
	bool squattingDest = false;

	if ( !spaceTree ) return false;
	RenderComponent* render = parentChit->GetRenderComponent();
	if ( !render ) return false;
	if ( (render->GetFlags() & MODEL_USER_AVOIDS ) == 0 ) return false;
	
	Rectangle2F bounds;
	bounds.Set( pos2.x-PATH_AVOID_DISTANCE, pos2.y-PATH_AVOID_DISTANCE, 
		        pos2.x+PATH_AVOID_DISTANCE, pos2.y+PATH_AVOID_DISTANCE );
	Model* root = spaceTree->QueryRect( bounds, MODEL_USER_AVOIDS, 0 );

	if ( root ) {
		Vector3F pos3    = { pos2.x, 0, pos2.y };
		float radius     = parentChit->GetRenderComponent()->RadiusOfBase();
		Vector3F avoid = { 0, 0 };
		static const Vector3F UP = { 0, 1, 0 };

		Vector3F wayPoint   = { path[pathPos].x, 0, path[pathPos].y };
		Vector3F destNormal = wayPoint - pos3;
		destNormal.SafeNormalize( 1, 0, 0 );

		while( root ) {
			Chit* chit = root->userData;
			if ( chit && chit != parentChit ) {
				
				Vector3F itPos3 = chit->GetSpatialComponent()->GetPosition();
				float itRadius  = chit->GetRenderComponent()->RadiusOfBase(); 

				float d = (pos3-itPos3).Length();
				float r = radius + itRadius;

				if ( d < r ) {
					avoidForceApplied = true;

					// Move away from the centers so the bases don't overlap.
					Vector3F normal = pos3 - itPos3;
					normal.y = 0;
					normal.SafeNormalize( -destNormal.x, -destNormal.y, -destNormal.z );
					float alignment = DotProduct( -normal, destNormal ); // how "in the way" is this?
					
					// Is this guy squatting on our dest?
					// This check is surprisingly broad, but keeps everyone from "orbiting"
					if ( (wayPoint-itPos3).LengthSquared() < r*r ) {
						// Dang squatter.
						if ( pathPos < nPathPos-1 ) {
							++pathPos;	// go around
							break;		// and stop avoiding since we are changing dest.
						}
						else {
							squattingDest = true;
						}
					}
					else {
						float mag = Min( r-d, Travel( MOVE_SPEED, delta ) ); 
						normal.Multiply( mag );
						avoid += normal;
				
						// Apply a sidestep vector so they don't just push.
						if ( alignment > 0.7f ) {
							Vector3F right;
							CrossProduct( destNormal, UP, &right );
							avoid += right * (0.5f*mag );
						}	
					}
				}
			}
			root = root->next;
		}
		avoid.y = 0;	// be sure.
		pos2.x += avoid.x;
		pos2.y += avoid.z;
	}
	return squattingDest;
}


void PathMoveComponent::ApplyBlocks()
{
	GRINLIZ_PERFTRACK;
	RenderComponent* render = parentChit->GetRenderComponent();

	Vector2F newPos2 = pos2;
	float rotation = 0;
	float radius = render->RadiusOfBase();

	WorldMap::BlockResult result = map->ApplyBlockEffect( pos2, radius, &newPos2 );
	blockForceApplied = ( result == WorldMap::FORCE_APPLIED );
	isStuck = ( result == WorldMap::STUCK );
	
	pos2 = newPos2;
}


void PathMoveComponent::DoTick( U32 delta )
{
	GRINLIZ_PERFTRACK;

	if ( queuedDest.x >= 0 ) {
		pathPos = nPathPos = 0;
		// Oh the callback is painful. The queuedDest will
		// get set by the callback from ComputeDest
		Vector2F q = queuedDest;
		queuedDest.Set( -1, -1 );	// clear here, so it can be set again
		ComputeDest( q );
	}

	blockForceApplied = false;
	avoidForceApplied = false;

	if ( pathPos < nPathPos ) {
		GetPosRot( &pos2, &rot );
		int startPathPos = pathPos;
		float distToNext = GetDistToNext2( pos2 );

		if ( rotationFirst )
			RotationFirst( delta );
		else
			MoveFirst( delta );

		bool squattingDest = AvoidOthers( delta );
		ApplyBlocks();
		SetPosRot( pos2, rot );

		// Position set: nothing below can change.

		if ( squattingDest ) {
			GLASSERT( pathPos >= nPathPos-1 );
			pathPos = nPathPos;
		}
		else {
			// Do we need to repath because we're stuck?
			if (    blockForceApplied  
				 && startPathPos == pathPos
				 && GetDistToNext2( pos2 ) >= distToNext ) 
			{ 
				++repath;
			}
			else {
				repath = 0;
			}
			if ( repath == 3 ) {
#ifdef DEBUG_PMC
				GLOUTPUT(( "Repath\n" ));
#endif
				GLASSERT( dest.x >= 0 );
				QueueDest( dest );
				repath = 0;
			}
		}
		// Are we at the end of the path data?
		if ( nPathPos > 0 && pathPos == nPathPos ) {
			if ( squattingDest || dest.Equal( path[nPathPos-1], parentChit->GetRenderComponent()->RadiusOfBase()*0.2f ) ) {
#ifdef DEBUG_PMC
				GLOUTPUT(( "Dest reached. squatted=%s\n", squattingDest ? "true" : "false" ));
#endif
				// actually reached the end!
				SendMessage( "PathMoveComponent", MSG_DESTINATION_REACHED );
				SetNoPath();
				GLASSERT( queuedDest.x > 0 );	// DEBUGGING
			}
			else {
				// continue path:
				GLASSERT( dest.x >= 0 );
				QueueDest( dest );
			}
		}
	}
	GLASSERT( (pathPos < nPathPos ) || queuedDest.x >= 0 );
}


void PathMoveComponent::DebugStr( grinliz::GLString* str )
{
	if ( pathPos < nPathPos ) {
		str->Format( "[PathMove]=%.1f,%.1f ", dest.x, dest.y );
	}
	else {
		str->Format( "[PathMove]=still " );
	}
}
