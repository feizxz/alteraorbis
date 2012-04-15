#include "worldmap.h"
#include "../grinliz/glutil.h"
#include "../engine/texture.h"

using namespace grinliz;

WorldMap::WorldMap( int width, int height ) : Map( width, height )
{
	GLASSERT( width % ZONE_SIZE == 0 );
	GLASSERT( height % ZONE_SIZE == 0 );

	GLASSERT( sizeof(Grid) == 4 );
	grid = new Grid[width*height];
	zoneInit = new U8[width*height/ZONE_SIZE2];
	memset( grid, 0, width*height*sizeof(Grid) );
	memset( zoneInit, 0, sizeof(*zoneInit)*width*height/ZONE_SIZE2 );

	texture[0] = TextureManager::Instance()->GetTexture( "map_water" );
	texture[1] = TextureManager::Instance()->GetTexture( "map_land" );
	pather = new micropather::MicroPather( this );
}


WorldMap::~WorldMap()
{
	delete [] grid;
	delete [] zoneInit;
	delete pather;
}


void WorldMap::InitCircle()
{
	memset( grid, 0, width*height*sizeof(Grid) );
	memset( zoneInit, 0, sizeof(*zoneInit)*width*height/ZONE_SIZE2 );

	const int R = Min( width, height )/2;
	const int R2 = R * R;
	const int cx = width/2;
	const int cy = height/2;

	for( int y=0; y<height; ++y ) {
		for( int x=0; x<width; ++x ) {
			int r2 = (x-cx)*(x-cx) + (y-cy)*(y-cy);
			if ( r2 < R2 ) {
				int i = INDEX( x, y );
				grid[i].isLand = TRUE;
			}
		}
	}
	Tessellate();
}


void WorldMap::Tessellate()
{
	for( int i=0; i<LOWER_TYPES; ++i ) {
		vertex[i].Clear();
		index[i].Clear();
	}
	// Could be much fancier: instead of just strips,
	// expand into 2 directions.
	//
	// 1   3
	// 0   2
	for( int j=0; j<height; ++j ) {
		int i=0;
		while ( i < width ) {
			int layer = grid[INDEX(i,j)].isLand;

			U16* pi = index[layer].PushArr( 6 );
			int base = vertex[layer].Size();
			pi[0] = base;
			pi[1] = base+3;
			pi[2] = base+2;
			pi[3] = base;
			pi[4] = base+1;
			pi[5] = base+3;

			PTVertex* pv = vertex[layer].PushArr( 2 );
			pv[0].pos.Set( (float)i, 0, (float)j );
			pv[0].tex.Set( 0, 0 );
			pv[1].pos.Set( (float)i, 0, (float)(j+1) );
			pv[1].tex.Set( 0, 1 );

			int w = 1;
			++i;
			while( i<width && grid[INDEX(i,j)].isLand == layer ) {
				++i;
				++w;
			}
			pv = vertex[layer].PushArr( 2 );
			pv[0].pos.Set( (float)i, 0, (float)j );
			pv[0].tex.Set( (float)w, 0 );
			pv[1].pos.Set( (float)i, 0, (float)(j+1) );
			pv[1].tex.Set( (float)w, 1 );
		}
	}
}


void WorldMap::SetBlock( const grinliz::Rectangle2I& pos )
{
	for( int y=pos.min.y; y<=pos.max.y; ++y ) {
		for( int x=pos.min.x; x<=pos.max.x; ++x ) {
			int i = INDEX( x, y );
			GLASSERT( grid[i].isBlock == FALSE );
			grid[i].isBlock = TRUE;
			zoneInit[ZDEX( x, y )] = 0;
			GLOUTPUT(( "Block (%d,%d) index=%d zone=%d set\n", x, y, i, ZDEX(x,y) ));
		}
	}
	pather->Reset();
}


void WorldMap::ClearBlock( const grinliz::Rectangle2I& pos )
{
	for( int y=pos.min.y; y<=pos.max.y; ++y ) {
		for( int x=pos.min.x; x<=pos.max.x; ++x ) {
			int i = INDEX( x, y );
			GLASSERT( grid[i].isBlock );
			grid[i].isBlock = FALSE;
			zoneInit[ZDEX( x, y )] = 0;
		}
	}
	pather->Reset();
}


bool WorldMap::JointPassable( int x0, int y0, int x1, int y1 )
{
	for( int x=x0; x<=x1; ++x ) {
		if ( !grid[INDEX(x,y1)].IsPassable() || grid[INDEX(x,y1)].isPathInit )
			return false;
	}
	for( int y=y0; y<y1; ++y ) {
		if ( !grid[INDEX(x1,y)].IsPassable() || grid[INDEX(x1,y)].isPathInit )
			return false;
	}
	return true;
}


bool WorldMap::RectPassable( int x0, int y0, int x1, int y1 )
{
	for( int y=y0; y<=y1; ++y ) {
		for( int x=x0; x<=x1; ++x ) {
			if ( !grid[INDEX(x,y)].IsPassable() || grid[INDEX(x,y)].isPathInit )
				return false;
		}
	}
	return true;
}


void WorldMap::CalcZone( int zx, int zy )
{
	zx = zx & (~(ZONE_SIZE-1));
	zy = zy & (~(ZONE_SIZE-1));

	if ( zoneInit[ZDEX(zx,zy)] == 0 ) {
		GLOUTPUT(( "CalcZone (%d,%d) %d\n", zx, zy, ZDEX(zx,zy) ));
		zoneInit[ZDEX(zx,zy)] = 1;

		for( int y=zy; y<zy+ZONE_SIZE; ++y ) {
			for( int x=zx; x<zx+ZONE_SIZE; ++x ) {
				grid[INDEX(x,y)].isPathInit = FALSE;
			}
		}

		int xEnd = zx + ZONE_SIZE;
		int yEnd = zy + ZONE_SIZE;

		for( int y=zy; y<yEnd; ++y ) {
			for( int x=zx; x<xEnd; ++x ) {

				int index = INDEX(x,y);
				if ( !grid[index].IsPassable() )
					continue;
				if ( grid[index].isPathInit )
					continue;

				// Expand the square.
				int size=1;
				while( x+size < xEnd && y+size < yEnd && JointPassable( x, y, x+size, y+size ) ) {
					++size;
				}
				// How far could we go on the x axis?
				int xSize = size;
				while(    x+xSize < xEnd 
					   //&& ( xSize <= size*2 )
					   && RectPassable( x+xSize, y, x+xSize, y+size-1 ) ) {
					++xSize;
				}
				// How far on the y axis?
				int ySize = size;
				while(    y+ySize < yEnd 
					  // && (ySize <= size*2 )
					   && RectPassable( x, y+ySize, x+size-1, y+ySize ) ) {
					++ySize;
				}

				// Choose the better of the x or y axis
				if ( ySize > xSize ) {
					xSize = size;
				}
				else {
					ySize = size;
				}

				//GLOUTPUT(( "sub-zone (%d,%d) size=(%d,%d)\n", x, y, xSize, ySize ));
				// Write the changes back
				for( int j=y; j<y+ySize; ++j ) {
					for( int i=x; i<x+xSize; ++i ) {
						GLASSERT( !grid[INDEX(i,j)].isPathInit );
						grid[ INDEX( i, j ) ].SetPathOrigin( i-x, j-y, xSize, ySize );
					}
				}
			}
		}
	}
}


float WorldMap::LeastCostEstimate( void* stateStart, void* stateEnd )
{
	int startX, startY, endX, endY;

	Grid gStart = ToGrid( stateStart, &startX, &startY );
	Grid gEnd =   ToGrid( stateEnd, &endX, &endY );

	Vector2F start = { (float)startX + (float)gStart.sizeX * 0.5f,
	                   (float)startY + (float)gStart.sizeY * 0.5f };
	Vector2F end   = { (float)endX + (float)gEnd.sizeX * 0.5f,
					   (float)endY + (float)gEnd.sizeY * 0.5f };
	return (end-start).Length();
}


void WorldMap::AdjacentCost( void* state, MP_VECTOR< micropather::StateCost > *adjacent )
{
	int startX, startY;
	Grid gStart = ToGrid( state, &startX, &startY );
	Vector2F start = { (float)startX + (float)gStart.sizeX * 0.5f,
	                   (float)startY + (float)gStart.sizeY * 0.5f };

	Rectangle2I bounds = this->Bounds();
	Vector2I currentSubZone = { -1, -1 };

	Vector2I adj[ZONE_SIZE*4+4];
	int num=0;

	int dx = (int)gStart.sizeX;
	int dy = (int)gStart.sizeY;

	for( int x=startX; x < startX+dx; ++x )		{ adj[num++].Set( x, startY-1 ); }
	if (    bounds.Contains( startX+dx, startY-1 )
		 && grid[INDEX(startX+dx-1, startY-1)].IsPassable()
		 && grid[INDEX(startX+dx,   startY-1)].IsPassable()
		 && grid[INDEX(startX+dx,   startY)].IsPassable() )
		{ adj[num++].Set( startX+dx, startY-1 ); }
	for( int y=startY; y < startY+dy; ++y )		{ adj[num++].Set( startX+gStart.sizeX, y ); }
	if (    bounds.Contains( startX+dx, startY+dy )
		 && grid[INDEX(startX+dx,   startY+dy-1)].IsPassable()
		 && grid[INDEX(startX+dx,   startY+dy)].IsPassable()
		 && grid[INDEX(startX+dx-1, startY+dy)].IsPassable() )
		{ adj[num++].Set( startX+dx, startY+dy ); }
	for( int x=startX+(int)gStart.sizeX-1; x >= startX; --x )	{ adj[num++].Set( x, startY+gStart.sizeY ); }
	if (    bounds.Contains( startX-1, startY+dy )
		 && grid[INDEX(startX,   startY+dy)].IsPassable()
		 && grid[INDEX(startX-1, startY+dy)].IsPassable()
		 && grid[INDEX(startX-1, startY+dy-1)].IsPassable() )
		{ adj[num++].Set( startX-1, startY+dy ); }
	for( int y=startY+(int)gStart.sizeY-1; y >= startY; --y )	{ adj[num++].Set( startX-1, y ); }
	if (    bounds.Contains( startX-1, startY-1 )
		 && grid[INDEX(startX-1, startY)].IsPassable()
		 && grid[INDEX(startX-1, startY-1)].IsPassable()
		 && grid[INDEX(startX,   startY-1)].IsPassable() )
		{ adj[num++].Set( startX-1, startY-1 ); }

	for( int i=0; i<num; ++i ) {
		int x = adj[i].x;
		int y = adj[i].y;

		if ( bounds.Contains( x, y ) ) {
			Vector2I subV = GetSubZone( x, y );
			if ( subV.x >= 0 && subV != currentSubZone ) {
				Grid g = grid[INDEX(subV.x,subV.y)];
				Vector2F end = { (float)subV.x + (float)g.sizeX * 0.5f,
								 (float)subV.y + (float)g.sizeY * 0.5f };
				float cost = (end-start).Length();
				
				micropather::StateCost sc = { ToState( subV.x, subV.y ), cost };
				adjacent->push_back( sc );
				currentSubZone = subV;
			}
		}
	}
}


void WorldMap::PrintStateInfo( void* state )
{
	int startX, startY;
	Grid gStart = ToGrid( state, &startX, &startY );
	GLOUTPUT(( "(%d,%d) ", startX, startY ));	
}


void WorldMap::ShowAdjacent( float _x, float _y )
{
	int x = (int)_x;
	int y = (int)_y;
	for( int i=0; i<width*height; ++i ) {
		grid[i].debug_adjacent = 0;
		grid[i].debug_origin = 0;
	}
	if ( grid[INDEX(x,y)].IsPassable() ) {
		Vector2I sub = GetSubZone( x, y );
		grid[INDEX(sub.x,sub.y)].debug_origin = TRUE;

		MP_VECTOR< micropather::StateCost > adjacent;
		AdjacentCost( ToState( sub.x, sub.y ), &adjacent );
		for( unsigned i=0; i<adjacent.size(); ++i ) {
			Vector2I a;
			ToGrid( adjacent[i].state, &a.x, &a.y );
			grid[INDEX(a.x,a.y)].debug_adjacent = TRUE;
		}
	}
}


void WorldMap::DrawZones()
{
	CompositingShader debug( GPUShader::BLEND_NORMAL );
	debug.SetColor( 1, 1, 1, 0.5f );
	CompositingShader debugOrigin( GPUShader::BLEND_NORMAL );
	debugOrigin.SetColor( 1, 0, 0, 0.5f );
	CompositingShader debugAdjacent( GPUShader::BLEND_NORMAL );
	debugAdjacent.SetColor( 1, 1, 0, 0.5f );

	for( int j=0; j<height; ++j ) {
		for( int i=0; i<width; ++i ) {
			if ( !zoneInit[ZDEX(i,j)] ) {
				CalcZone( i, j );
			}

			static const float offset = 0.2f;
			Grid g = grid[INDEX(i,j)];
			
			if ( g.IsPassable() && g.isPathInit && g.deltaXOrigin == 0 && g.deltaYOrigin == 0 ) {
				int xSize = g.sizeX;
				int ySize = g.sizeY;

				Vector3F p0 = { (float)i+offset, 0.1f, (float)j+offset };
				Vector3F p1 = { (float)(i+xSize)-offset, 0.1f, (float)(j+ySize)-offset };

				if ( g.debug_origin ) {
					debugOrigin.DrawQuad( p0, p1, false );
				}
				else if ( g.debug_adjacent ) {
					debugAdjacent.DrawQuad( p0, p1, false );
				}
				else {
					debug.DrawQuad( p0, p1, false );
				}
			}
		}
	}
}


void WorldMap::Draw3D(  const grinliz::Color3F& colorMult, GPUShader::StencilMode mode )
{
	GPUStream stream;

	stream.stride = sizeof( PTVertex );

	stream.posOffset = PTVertex::POS_OFFSET;
	stream.nPos = 3;
	stream.texture0Offset = PTVertex::TEXTURE_OFFSET;
	stream.nTexture0 = 2;

	// Real code to draw the map:
	FlatShader shader;
	shader.SetColor( colorMult.r, colorMult.g, colorMult.b );
	shader.SetStencilMode( mode );
	for( int i=0; i<LOWER_TYPES; ++i ) {
		shader.SetStream( stream, vertex[i].Mem(), index[i].Size(), index[i].Mem() );
		shader.SetTexture0( texture[i] );
		shader.Draw();
	}

	// Debugging pathing zones:
	DrawZones();
	// Debugging coordinate system:
	Vector3F origin = { 0, 0.1f, 0 };
	Vector3F xaxis = { 5, 0, 0 };
	Vector3F zaxis = { 0, 0.1f, 5 };

	FlatShader debug;
	debug.SetColor( 1, 0, 0, 1 );
	debug.DrawArrow( origin, xaxis, false );
	debug.SetColor( 0, 0, 1, 1 );
	debug.DrawArrow( origin, zaxis, false );
}

