#ifndef LUMOS_WORLD_GRID_INCLUDED
#define LUMOS_WORLD_GRID_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glcolor.h"

static const int MAX_ROCK_HEIGHT		= 3;

struct WorldGrid {

private:
	// memset(0) should work, and make it water.

	unsigned isLand				: 1;
	unsigned isGrid				: 1;
	unsigned isPort				: 1;

	unsigned isBlocked			: 1;	// blocks the pather; implies inUse
	unsigned inUse				: 1;	// used, but the pather isn't blocked

	unsigned magma				: 1;	// land, rock, or water can be set to magma

	unsigned zoneSize			: 6;	// 0-31
	unsigned nominalRockHeight	: 2;	// 0-3
	unsigned rockHeight			: 2;
	unsigned poolHeight			: 2;

	unsigned debugAdjacent		: 1;
	unsigned debugPath			: 1;
	unsigned debugOrigin		: 1;

public:
	grinliz::Color4U8 ToColor() const {
		grinliz::Color4U8 c = { 0, 0, 0, 255 };

		if ( isGrid ) {
			c.Set( 120, 180, 180, 255 );
		}
		else if ( isPort ) {
			c.Set( 0, 120, 0, 255 );
		}
		else if ( isLand ) {
			c.Set( 0, 140+nominalRockHeight*20, 0, 255 );
		}
		else {
			c.Set( 0, 0, 200, 255 );
		}
		return c;
	}

	bool IsLand() const			{ return isLand != 0; }
	void SetLand( bool land )	{ if ( land ) SetLand(); else SetWater(); }

	void SetGrid()				{ isLand = 1; isBlocked = 1; isGrid = 1; }
	void SetPort()				{ isLand = 1; inUse = 1; isPort = 1; }
	void SetLandAndRock( U8 h )	{
		// So confusing. Max rock height=3, but land goes from 1-4 to be distinct from water.
		// Subtract here.
		if ( !h ) {
			SetWater();	
		}
		else {
			SetLand();
			SetNominalRockHeight( h-1 );
		}
	}

	void SetLand()				{ 
		GLASSERT( sizeof(WorldGrid) == sizeof(U32) ); 
		isLand = 1;
	}

	int NominalRockHeight() const { return nominalRockHeight; }
	void SetNominalRockHeight( int h ) {
		GLASSERT( IsLand() );
		GLASSERT( h >= 0 && h <= MAX_ROCK_HEIGHT );
		nominalRockHeight = h;
	}

	int RockHeight() const { return rockHeight; }
	void SetRockHeight( int h ) {
		GLASSERT( IsLand() || (h==0) );
		GLASSERT( h >= 0 && h <= MAX_ROCK_HEIGHT );
		rockHeight = h;
	}

	int PoolHeight() const { return poolHeight; }
	void SetPoolHeight( int p ) {
		GLASSERT( IsLand() || (p==0) );
		GLASSERT( !p || p > (int)rockHeight );
		poolHeight = p;
	}

	bool IsWater() const		{ return !IsLand(); }
	void SetWater()				{ 
		isLand = 0;
		nominalRockHeight = 0;
	}

	bool Magma() const			{ return magma != 0; }
	void SetMagma( bool m )		{ magma = m ? 1 : 0; }

	bool IsPassable() const { 
		return IsLand() && !IsBlocked(); 
	}

	bool DebugAdjacent() const		{ return debugAdjacent != 0; }
	void SetDebugAdjacent( bool v ) { debugAdjacent = v ? 1 : 0; }

	bool DebugOrigin() const		{ return debugOrigin != 0; }
	void SetDebugOrigin( bool v )	{ debugOrigin = v ? 1 : 0; }

	bool DebugPath() const			{ return debugPath != 0; }
	void SetDebugPath( bool v )		{ debugPath = v ? 1 : 0; }

	bool IsBlocked() const			{ return isBlocked != 0; }
	void SetBlocked( bool block )	{ 
		GLASSERT( IsLand() );
		if (block) {
			GLASSERT( IsPassable() );
			isBlocked = 1;
		}
		else { 
			isBlocked = 0;
		}
	}

	bool InUse() const				{ return inUse != 0; }
	void SetInUse( bool use )		{
		if ( use ) {
			GLASSERT( !InUse() );
			inUse = 1;
		}
		else {
			GLASSERT( InUse() );
			inUse = 0;
		}
	}

	U32 ZoneSize() const			{ return zoneSize;}

	// x & y must be in zone.
	grinliz::Vector2I ZoneOrigin( int x, int y ) const {
		U32 mask = 0xffffffff;
		if ( zoneSize ) {
			mask = ~(zoneSize-1);
		}
		grinliz::Vector2I v = { x & mask, y & mask };
		return v;
	}

	void SetZoneSize( int s )	{ 
		zoneSize = s;
		GLASSERT( s == zoneSize );
	}
};

#endif // LUMOS_WORLD_GRID_INCLUDED
