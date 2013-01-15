#ifndef LUMOS_WORLD_GRID_INCLUDED
#define LUMOS_WORLD_GRID_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glcolor.h"

struct WorldGrid {
private:
	// memset(0) should work, and make it water.

	U8 pathColor;
	U8 pad0;
	U8 pad1;
	U8 pad2;

	unsigned isLand				: 1;
	unsigned isBlocked			: 1;

	unsigned zoneSize			: 6;	// 0-31

	unsigned debugAdjacent		: 1;
	unsigned debugPath			: 1;
	unsigned debugOrigin		: 1;

public:
	grinliz::Color4U8 ToColor() const {
		grinliz::Color4U8 c = {
			0,
			(isLand * 0x80) | (pathColor & 0x7f),
			(1-isLand) * 0xff,
			255
		};
		return c;
	}

	bool IsLand() const			{ return isLand != 0; }
	void SetLand( bool land )	{ if ( land ) SetLand(); else SetWater(); }
	void SetLand()				{ 
		GLASSERT( sizeof(WorldGrid) == 2*sizeof(U32) ); 
		isLand = 1;
	}

	bool IsWater() const		{ return !IsLand(); }
	void SetWater()				{ 
		GLASSERT( sizeof(WorldGrid) == 2*sizeof(U32) );
		isLand = 0;
	}

	U32  PathColor() const		{ return pathColor; }
	void SetPathColor( int c )	{ 
		GLASSERT( c >= 0 && c <= 255 ); 
		pathColor = c;
	}

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
