/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LUMOS_WORLD_MAP_INCLUDED
#define LUMOS_WORLD_MAP_INCLUDED

#include "gamelimits.h"

#include "../engine/map.h"
#include "../engine/rendertarget.h"

#include "../micropather/micropather.h"

#include "../grinliz/glrectangle.h"
#include "../grinliz/glcontainer.h"
#include "../grinliz/glmemorypool.h"
#include "../grinliz/glbitarray.h"

class Texture;

/*
	The world map has layers. (May add more in the future.)
	Basic layer: water or land.
	For land, is it passable or not?
	In the future, flowing water, magma, etc. would be great.

	Rendering is currently broken into strips. (Since the map
	doesn't change after the Init...() is called.) Could be
	further optimized to use regions.
*/
class WorldMap : public Map, 
	             public micropather::Graph
{
public:
	WorldMap( int width, int height );
	~WorldMap();

	void InitCircle();
	bool InitPNG( const char* filename, 
				  grinliz::CDynArray<grinliz::Vector2I>* blocks,
				  grinliz::CDynArray<grinliz::Vector2I>* wayPoints,
				  grinliz::CDynArray<grinliz::Vector2I>* features );

	void SetBlock( int x, int y )	{ grinliz::Rectangle2I pos; pos.Set( x, y, x, y ); SetBlock( pos ); }
	void SetBlock( const grinliz::Rectangle2I& pos );
	void ClearBlock( int x, int y )	{ grinliz::Rectangle2I pos; pos.Set( x, y, x, y ); ClearBlock( pos ); }
	void ClearBlock( const grinliz::Rectangle2I& pos );

	bool IsBlockSet( int x, int y ) { return grid[INDEX(x,y)].isBlock != 0; }
	bool IsLand( int x, int y )		{ return grid[INDEX(x,y)].isLand != 0; }
	
	// Call the pather; return true if successful.
	bool CalcPath(	const grinliz::Vector2F& start, 
					const grinliz::Vector2F& end, 
					grinliz::CDynArray<grinliz::Vector2F> *path,
					float* totalCost,
					bool showDebugging = false );
	bool CalcPath(	const grinliz::Vector2F& start, 
					const grinliz::Vector2F& end, 
					grinliz::Vector2F *path,
					int *pathLen,
					int maxPath,
					float* totalCost,
					bool showDebugging = false );

	enum BlockResult {
		NO_EFFECT,
		FORCE_APPLIED,
		STUCK
	};

	// Calculate the effect of walls on 'pos'. Note that
	// there can be multiple walls, and this takes multiple calls.
	BlockResult CalcBlockEffect(	const grinliz::Vector2F& pos,
									float radius,
									grinliz::Vector2F* force );
	// Call CalcBlockEffect and return the result.
	BlockResult ApplyBlockEffect(	const grinliz::Vector2F inPos, 
									float radius, 
									grinliz::Vector2F* outPos );

	// ---- Map ---- //
	virtual void Submit( GPUState* shader, bool emissiveOnly );
	virtual void Draw3D(  const grinliz::Color3F& colorMult, GPUState::StencilMode );

	// ---- MicroPather ---- //
	virtual float LeastCostEstimate( void* stateStart, void* stateEnd );
	virtual void AdjacentCost( void* state, MP_VECTOR< micropather::StateCost > *adjacent );
	virtual void  PrintStateInfo( void* state );

	// --- Debugging -- //
	void ShowAdjacentRegions( float x, float y );
	void ShowRegionPath( float x0, float y0, float x1, float y1 );
	void ShowRegionOverlay( bool over ) { debugRegionOverlay = over; }
	float PatherCache();
	void PatherCacheHitMiss( int* hits, int* miss, float* ratio ) { *hits=0; *miss=0; *ratio=1; }
	int CalcNumRegions();

private:
	int INDEX( int x, int y ) const { 
		GLASSERT( x >= 0 && x < width ); GLASSERT( y >= 0 && y < height ); 
		return y*width + x; 
	}
	int INDEX( grinliz::Vector2I v ) const { return INDEX( v.x, v.y ); }

	int ZDEX( int x, int y ) const { 
		GLASSERT( x >= 0 && x < width ); GLASSERT( y >= 0 && y < height );
		x /= ZONE_SIZE;
		y /= ZONE_SIZE;
		return (y*width/ZONE_SIZE) + x; 
	} 

	void Init( int w, int h );
	void Tessellate();
	void CalcZone( int x, int y );

	bool DeleteRegion( int x, int y );	// surprisingly complex
	void DeleteAllRegions();

	void DrawZones();			// debugging
	void ClearDebugDrawing();	// debugging
	void DumpRegions();

	/* A 16x16 zone, needs 3 bits to describe the depth. From the depth
	   can infer the origin, etc.
		d=0 16
		d=1 8
		d=2 4
		d=3 2
		d=4 1
	*/
	enum {
		TRUE		= 1,
		FALSE		= 0,
		ZONE_SIZE	= 16,		// adjust size of bit fields to be big enough to represent this
		ZONE_SHIFT  = 4,
		ZONE_SIZE2  = ZONE_SIZE*ZONE_SIZE,
	};
	
	struct Grid {
		U32 isLand			: 1;
		U32 isBlock			: 1;
		U32 color			: 8;	// zone color
		U32 size			: 5;	// if passable, size of the region
		U32 debug_adjacent  : 1;
		U32 debug_origin    : 1;
		U32 debug_path		: 1;

		bool IsPassable() const			{ return isLand == TRUE && isBlock == FALSE; }
	};

	// The solver has 3 components:
	//	Vector path:	the final result, a collection of points that form connected vector
	//					line segments.
	//	Grid path:		intermediate; a path checked by a step walk between points on the grid
	//  Region path:	the micropather computed region

	// Call the region solver. Put the result in the pathVector
	//int  RegionSolve( Region* start, Region* end, float* totalCost );
	bool GridPath( const grinliz::Vector2F& start, const grinliz::Vector2F& end );

	Grid* GridAt( int x, int y ) {
		GLASSERT( grid );
		GLASSERT( x >= 0 && x < width );
		GLASSERT( y >= 0 && y < height );
		return grid + y*width + x;
	}

	grinliz::Vector2I RegionOrigin( int x, int y ) {
		Grid* g = GridAt( x, y );
		U32 size = g->size;
		U32 mask = ~(size-1);
		grinliz::Vector2I v = { x&mask, y&mask };
		return v;
	}

	bool IsRegionOrigin( int x, int y ) { 
		Grid* g = GridAt( x, y );
		U32 size = g->size;
		U32 mask = ~(size-1);
		return (x == (x&mask)) && (y == (y&mask));
	}

	grinliz::Vector2F RegionCenter( int x, int y ) {
		Grid* g = GridAt( x, y );
		grinliz::Vector2I v = RegionOrigin( x, y );
		float half = (float)g->size * 0.5f;
		grinliz::Vector2F c = { (float)(x) + half, (float)y + half };
		return c;
	}

	grinliz::Rectangle2F RegionBounds( int x, int y ) {
		Grid* g = GridAt( x, y );
		grinliz::Vector2I v = RegionOrigin( x, y );
		grinliz::Rectangle2F b;
		b.min.Set( (float)v.x, (float)v.y );
		b.max.Set( (float)(v.x + g->size), (float)(v.y + g->size) );
		return b;
	}
		
	void* ToState( int x, int y ) {
		Grid* g = GridAt( x, y );
		U32 size = g->size;
		U32 mask = ~(size-1);
		return (void*)(y*width+x);
	}

	Grid* ToGrid( void* state, grinliz::Vector2I* vec ) {
		int v = (int)(state);
		int y = v / width;
		int x = v - y*width;
		if ( vec ) {
			vec->Set( x, y );
		}
		return GridAt( x, y );
	}

	Grid* grid;		// pathing info.
	micropather::MicroPather *pather;
	bool debugRegionOverlay;

	MP_VECTOR< void* >						pathRegions;
	grinliz::CDynArray< grinliz::Vector2F >	debugPathVector;
	grinliz::CDynArray< grinliz::Vector2F >	pathCache;

	enum {
		LOWER_TYPES = 2		// land or water
	};
	Texture*						texture[LOWER_TYPES];
	grinliz::CDynArray<PTVertex>	vertex[LOWER_TYPES];
	grinliz::CDynArray<U16>			index[LOWER_TYPES];
	grinliz::BitArray< MAX_MAP_SIZE/ZONE_SIZE, MAX_MAP_SIZE/ZONE_SIZE, 1 > zoneInit;
};

#endif // LUMOS_WORLD_MAP_INCLUDED
