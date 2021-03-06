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

#include "worldmap.h"

#include "../version.h"

#include "../xegame/xegamelimits.h"
#include "../xegame/chitevent.h"
#include "../xegame/chitbag.h"
#include "../xegame/platformpath.h"
#include "../xegame/spatialcomponent.h"

#include "../grinliz/glutil.h"
#include "../grinliz/glgeometry.h"
#include "../grinliz/glcolor.h"
#include "../Shiny/include/Shiny.h"
#include "../grinliz/glperformance.h"

#include "../xarchive/glstreamer.h"
#include "../shared/lodepng.h"
#include "../xarchive/squisher.h"

#include "../engine/engine.h"
#include "../engine/texture.h"
#include "../engine/ufoutil.h"
#include "../engine/surface.h"
#include "../engine/loosequadtree.h"
#include "../engine/particle.h"

#include "worldinfo.h"
#include "gameitem.h"
#include "lumosgame.h"
#include "gameitem.h"
#include "fluidsim.h"
#include "circuitsim.h"
#include "physicssims.h"

#include "../script/worldgen.h"
#include "../script/procedural.h"
#include "../script/itemscript.h"
#include "../script/plantscript.h"


using namespace grinliz;
using namespace micropather;
using namespace tinyxml2;

static const float PLANT_SATURATION = 1.0f;
static const float GRID_SATURATION = 1.0f;

// Startup for test world
// Baseline:				15,000
// Coloring regions:		 2,300
// Switch to 'struct Region' 2,000
// Region : public PathNode	 1,600
// Bug fix: incorrect recusion   4	yes, 4
// 

static const Vector2I DIR_I4[4] = {
	{ 1, 0 },
	{ 0, 1 },
	{ -1, 0 },
	{ 0, -1 },
};

static const Vector2I DIR_I8[8] = {
	{ 1, 0 },
	{ 1, 1 },
	{ 0, 1 },
	{ -1, 1 },
	{ -1, 0 },
	{ -1, -1 },
	{ 0, -1 },
	{ 1, -1 }
};

static const Vector2F DIR_F8[8] = {
	{ 1, 0 },
	{ 1, 1 },
	{ 0, 1 },
	{ -1, 1 },
	{ -1, 0 },
	{ -1, -1 },
	{ 0, -1 },
	{ 1, -1 }
};


static const float LEN_F8[8] = {
	1, SQRT2, 1, SQRT2, 1, SQRT2, 1, SQRT2
};

static const float INV_LEN_F8[8] = {
	1, INV_SQRT2, 1, INV_SQRT2, 1, INV_SQRT2, 1, INV_SQRT2
};


WorldMap::WorldMap(int width, int height) : Map(width, height)
{
	GLASSERT( width % ZONE_SIZE == 0 );
	GLASSERT( height % ZONE_SIZE == 0 );
	ShaderManager::Instance()->AddDeviceLossHandler( this );

	engine = 0;
	physics = 0;
	worldInfo = 0;
	slowTick = SLOW_TICK;
	iMapGridUse = 0;
	processIndex = 0;

	voxelTexture = 0;
	gridTexture = 0;

	debugRegionOverlay.Set( 0, 0, 0, 0 );

	voxelVertexVBO = 0;
	gridVertexVBO = 0;
	newsHistory = 0;

	magmaGrids.Reserve(1000);
	treePool.Reserve(1000);

	memset(grid, 0, sizeof(WorldGrid)*MAX_MAP_SIZE*MAX_MAP_SIZE);
	memset(pathers, 0, sizeof(pathers[0]) * NUM_SECTORS_2);

	for (int i = 0; i < NUM_EXTENDED_PLANT_TYPES; ++i) {
		for (int j = 0; j < MAX_PLANT_STAGES; ++j) {
			plantCount[i][j] = 0;
		}
	}

	// --- self call: make sure memory allocated. ---
	Init(width, height);
}


WorldMap::~WorldMap()
{
	GLASSERT( engine == 0 );
	ShaderManager::Instance()->RemoveDeviceLossHandler( this );

	DeleteAllRegions();
	delete worldInfo;

	delete voxelVertexVBO;
	delete gridVertexVBO;

	for (int i = 0; i < NUM_SECTORS_2; ++i) {
		delete pathers[i];
		pathers[i] = 0;
	}
}


void WorldMap::DeviceLoss()
{
	FreeVBOs();
}

void WorldMap::FreeVBOs()
{
	delete voxelVertexVBO;
	delete gridVertexVBO;
	voxelVertexVBO = 0;
	gridVertexVBO = 0;
}


const SectorData& WorldMap::GetSectorData( int x, int y ) const
{
	Vector2I sector = ToSector(x, y);
	return worldInfo->GetSectorData( sector );
}


const SectorData& WorldMap::GetSectorData( const Vector2I& sector ) const
{
	return worldInfo->GetSectorData( sector );
}


void WorldMap::AttachEngine( Engine* e, IMapGridBlocked* imap ) 
{
	GLASSERT( (e==0) || (e!=0 && engine==0) );

	if ( !e ) {
		for( int j=0; j<height; ++j ) {
			for( int i=0; i<width; ++i ) {
				if ( grid[INDEX(i,j)].IsLand() ) {
					SetRock( i, j, 0, false, 0 );
				}
			}
		}
	}
	if (e == 0) {
		// Tear down:
		if (voxelVertexVBO) {
			delete voxelVertexVBO;
			voxelVertexVBO = 0;
		}
		for (Model** it = treePool.Mem(); it < treePool.End(); ++it) {
			delete *it;
		}
		treePool.Clear();
	}

	engine = e;
	iMapGridUse = imap;
}


void WorldMap::AttatchPhysics(PhysicsSims* sims)
{
	GLASSERT(!physics || !sims || (physics == sims));
	physics = sims;
}


void WorldMap::VoxelHit( const Vector3I& v, const DamageDesc& dd )
{
	//Vector2I v2 = { v.x, v.z };
	int index = INDEX(v.x, v.z);
	const WorldGrid wasWG = grid[index];
	
	if (grid[index].RockHeight() || grid[index].Plant()) {
		// fluids don't take damage; rocks and plants do.
		grid[index].DeltaHP((int)(-dd.damage));
	}
	if ( grid[index].HP() == 0 ) {
		if (wasWG.HP()) {
			Vector3F pos = { (float)v.x + 0.5f, (float)v.y + 0.5f, (float)v.z + 0.5f };
			engine->particleSystem->EmitPD(ISC::derez, pos, V3F_UP, 0);
			SetRock(v.x, v.z, 0, false, 0);
			SetPlant(v.x, v.z, 0, 0);
		}
	}
	else if (grid[index].Plant()) {
		const GameItem* plant = PlantScript::PlantDef( grid[index].Plant() - 1);

		// catch fire/shock?
		float chanceFire = 0;
		float chanceShock = 0;
		if (dd.effects & GameItem::EFFECT_FIRE) {
			if (plant->flags & GameItem::IMMUNE_FIRE)
				chanceFire = 0;
			else if (plant->flags & GameItem::FLAMMABLE)
				chanceFire = CHANCE_FIRE_HIGH;
			else
				chanceFire = CHANCE_FIRE_NORMAL;
		}
		if (dd.effects & GameItem::EFFECT_SHOCK) {
			if (plant->flags & GameItem::IMMUNE_SHOCK)
				chanceShock = 0;
			else if (plant->flags & GameItem::SHOCKABLE)
				chanceShock = CHANCE_FIRE_HIGH;
			else
				chanceShock = CHANCE_FIRE_NORMAL;
		}
		bool fire = random.Uniform() < chanceFire;
		bool shock = random.Uniform() < chanceShock;
		if (fire || shock) {
			grid[index].SetOn(fire, shock);
		}
	}
}


void WorldMap::SavePNG( const char* filename )
{
	Color4U8* pixels = new Color4U8[width*height];

	for (int j = 0; j < height; ++j) {
		for (int i = 0; i < width; ++i) {
			pixels[j*width+i] = grid[INDEX(i,j)].ToColor();
		}
	}
	GLString path;
	GetSystemPath(GAME_SAVE_DIR, filename, &path);
	lodepng_encode32_file( path.c_str(), (const unsigned char*)pixels, width, height );

	delete [] pixels;
}


void WorldMap::Save( const char* filename )
{
	// Debug or laptap, about 4.5MClock
	// smaller window size: 3.8MClock
	// btype == 0 about the same.
	// None of this matters; may need to add an ultra-simple fast encoder.
	GLString path;
	GetSystemPath(GAME_SAVE_DIR, filename, &path);
	FILE* fp = fopen( path.c_str(), "wb" );
	GLASSERT( fp );
	if ( fp ) {

		StreamWriter writer(fp, CURRENT_FILE_VERSION);

		XarcOpen( &writer, "Map" );
		XARC_SER( &writer, width );
		XARC_SER( &writer, height );
		
		worldInfo->Serialize( &writer );
		XarcClose( &writer );

		// Tack on the grid so that the dat file can still be inspected.
		//fwrite( grid, sizeof(WorldGrid), width*height, fp );

		// This works very well; about 3:1 compression.
		Squisher squisher;
		squisher.StreamEncode( grid, sizeof(WorldGrid)*MAX_MAP_SIZE*MAX_MAP_SIZE, fp );
		squisher.StreamEncode( 0, 0, fp );

		fclose( fp );
	}
}


void WorldMap::Load( const char* filename )
{
	GLString path;
	GetSystemPath(GAME_SAVE_DIR, filename, &path);
	FILE* fp = fopen(path.c_str(), "rb");
	GLASSERT( fp );
	if ( fp ) {
		StreamReader reader( fp );
		
		XarcOpen( &reader, "Map" );

		XARC_SER(&reader, width);
		XARC_SER( &reader, height );
		Init( width, height );

		worldInfo->Serialize( &reader );
		XarcClose( &reader );

		Squisher squisher;
		squisher.StreamDecode( grid, sizeof(WorldGrid)*MAX_MAP_SIZE*MAX_MAP_SIZE, fp );

		fclose( fp );
		
		usingSectors = true;
		// Set up the rocks.
		for( int j=0; j<height; ++j ) {
			for( int i=0; i<width; ++i ) {
				int index = INDEX( i, j );
				grid[index].extBlock = 0;	// clear out the block. will be set by callback later.
				const WorldGrid& wg = grid[index];
				SetRock( i, j, -2, grid[index].Magma(), grid[index].RockType() );

				if (wg.Plant()) {
					plantCount[wg.Plant() - 1][wg.PlantStage()] += 1;
				}
			}
		}
	}
}


void WorldMap::PatherCacheHitMiss( const grinliz::Vector2I& sector, micropather::CacheData* data )
{
	MicroPather* pather = GetPather(sector, false);
	if ( pather ) {
		pather->GetCacheData( data );
	}
}


int WorldMap::CalcNumRegions()
{
	int count = 0;
	if ( grid ) {
		// Delete all the regions. Be careful to only
		// delete from the origin location.
		for( int j=0; j<height; ++j ) {	
			for( int i=0; i<width; ++i ) {
				if ( IsZoneOrigin( i, j )) {
					++count;
				}
			}
		}
	}
	return count;
}


void WorldMap::DumpRegions()
{
	if ( grid ) {
		for( int j=0; j<height; ++j ) {	
			for( int i=0; i<width; ++i ) {
				if ( IsPassable(i,j) && IsZoneOrigin(i, j)) {
					const WorldGrid& gs = grid[INDEX(i,j)];
					GLOUTPUT(( "Region %d,%d size=%d", i, j, gs.ZoneSize() ));
					GLOUTPUT(( "\n" ));
					(void)gs;
				}
			}
		}
	}
}


void WorldMap::DeleteAllRegions()
{
	zoneInit.ClearAll();
}


void WorldMap::Init( int w, int h )
{
	// Reset the voxels
	if ( engine ) {
		Engine* savedEngine = engine;
		IMapGridBlocked* savedIMap = iMapGridUse;
		AttachEngine( 0, 0 );
		AttachEngine( savedEngine, savedIMap );
	}

	DeleteAllRegions();
	this->width = w;
	this->height = h;
	memset( grid, 0, MAX_MAP_SIZE*MAX_MAP_SIZE*sizeof(WorldGrid) );
	
	delete worldInfo;
	worldInfo = new WorldInfo( grid, width, height );
}


void WorldMap::InitCircle()
{
	memset( grid, 0, MAX_MAP_SIZE*MAX_MAP_SIZE*sizeof(WorldGrid) );

	const int R = Min( width, height )/2-1;
	const int R2 = R * R;
	const int cx = width/2;
	const int cy = height/2;

	for( int y=0; y<height; ++y ) {
		for( int x=0; x<width; ++x ) {
			int r2 = (x-cx)*(x-cx) + (y-cy)*(y-cy);
			if ( r2 < R2 ) {
				int i = INDEX( x, y );
				grid[i].SetLand(WorldGrid::LAND);
			}
		}
	}
	//Tessellate();
}


bool WorldMap::InitPNG( const char* filename, 
						grinliz::CDynArray<grinliz::Vector2I>* blocks,
						grinliz::CDynArray<grinliz::Vector2I>* wayPoints,
						grinliz::CDynArray<grinliz::Vector2I>* features )
{
	unsigned char* pixels = 0;
	unsigned w=0, h=0;
	static const Color3U8 BLACK = { 0, 0, 0 };
	static const Color3U8 BLUE  = { 0, 0, 255 };
	static const Color3U8 RED   = { 255, 0, 0 };
	static const Color3U8 GREEN = { 0, 255, 0 };

	int error = lodepng_decode24_file( &pixels, &w, &h, filename );
	GLASSERT( error == 0 );
	if ( error == 0 ) {
		Init( w, h );
		int x = 0;
		int y = 0;
		//int color=0;
		for (unsigned j = 0; j < h; ++j) {
			for (unsigned i = 0; i < w; ++i) {

				int pIndex = j*w + i;
				int index = INDEX(i, j);

				Color3U8 c = { pixels[pIndex * 3 + 0], pixels[pIndex * 3 + 1], pixels[pIndex * 3 + 2] };
				Vector2I p = { x, y };
				if (c == BLACK) {
					grid[index].SetLand(WorldGrid::LAND);
					blocks->Push(p);
				}
				else if (c.r() == c.g() && c.g() == c.b()) {
					grid[index].SetLand(WorldGrid::LAND);
					//color = c.r();
				}
				else if (c == BLUE) {
					grid[index].SetWater();
				}
				else if (c == RED) {
					grid[index].SetLand(WorldGrid::LAND);
					wayPoints->Push(p);
				}
				else if (c == GREEN) {
					grid[index].SetLand(WorldGrid::LAND);
					features->Push(p);
				}
				++x;
				if (x == int(w)) {
					x = 0;
					++y;
				}
			}
		}
		free( pixels );
	}
	//Tessellate();
	return error == 0;
}


void WorldMap::MapInit( const U8* land, const U16* path )
{
	GLASSERT( grid );
	for( int i=0; i<width*height; ++i ) {
		int h = *(land + i);
		if ( h >= WorldGen::WATER && h <= WorldGen::LAND3 ) {
			grid[i].SetLandAndRock( h );
		}
		else if ( h == WorldGen::GRID ) {
			grid[i].SetLand(WorldGrid::GRID);
		}
		else if ( h == WorldGen::PORT ) {
			grid[i].SetLand(WorldGrid::PORT);
		}
		else if ( h == WorldGen::CORE ) {
			grid[i].SetLandAndRock(1);
		}
		else {
			GLASSERT( 0 );
		}
		grid[i].SetPath( path[i] );
	}
}


void WorldMap::PushQuad( int layer, int x, int y, int w, int h, CDynArray<PTVertex>* vertex, CDynArray<U16>* index ) 
{
	U16* pi = index->PushArr( 6 );
	int base = vertex->Size();

	pi[0] = base;
	pi[1] = base+3;
	pi[2] = base+2;
	pi[3] = base;
	pi[4] = base+1;
	pi[5] = base+3;

	PTVertex* pv = vertex->PushArr( 4 );
	pv[0].pos.Set( (float)x, 0, (float)y );
	pv[0].tex.Set( 0, 0 );

	pv[1].pos.Set( (float)x, 0, (float)(y+h) );
	pv[1].tex.Set( 0, (float)h );

	pv[2].pos.Set( (float)(x+w), 0, (float)(y) );
	pv[2].tex.Set( (float)w, 0 );

	pv[3].pos.Set( (float)(x+w), 0, (float)(y+h) );
	pv[3].tex.Set( (float)w, (float)h );
}

#if defined(WORLDMAP_THREADS)
int WorldMap::ScanEffects(void* p1, void*, void*, void*)
{
	ScanEffectsData* data = (ScanEffectsData*)p1;
#else
int WorldMap::ScanEffects(ScanEffectsData* data)
{
#endif
	static const int MAP2 = MAX_MAP_SIZE*MAX_MAP_SIZE;
	Rectangle2I b = data->worldMap->Bounds();
	b.Outset(-1);

	for (int i = 0; i < data->n; ++i) {
		const int index = (data->start + i) & (MAP2 - 1);
		WorldGrid* wg = &data->worldMap->grid[index];
		if (!wg->IsLand()) continue;

		const int y = (index >> MAP_Y_SHIFT);
		const int x = (index & MAP_X_MASK);
		const Vector2I pos2i = { x, y };

		if (!b.Contains(pos2i)) continue;

		// flammability is reflected in the chance
		// of it catching fire; once on fire, everything
		// has the same chance of the fire going out.

		if (wg->PlantOnFire()) {
			bool underWater = wg->FluidHeight() && (wg->FluidType() == WorldGrid::FLUID_WATER);
			if (underWater || (data->random.Uniform() < CHANCE_FIRE_OUT)) {
				wg->SetPlantOnFire(false);
			}
		}
		if (wg->PlantOnShock() && data->random.Uniform() < CHANCE_FIRE_OUT) {
			wg->SetPlantOnShock(false);
		}

		int effect = 0;
		if (wg->Plant()) {
			if (wg->PlantOnFire())  effect |= GameItem::EFFECT_FIRE;
			if (wg->PlantOnShock()) effect |= GameItem::EFFECT_SHOCK;
		}

		if (wg->Magma() || (wg->IsFluid() && wg->FluidType() == WorldGrid::FLUID_LAVA)) {
			effect |= GameItem::EFFECT_FIRE;
		}

		if (effect) {
			EffectRecord r = { pos2i, effect };
			data->effects->Push(r);
		}
	}
	return data->n;
}

/*
	2 systems in place:
	1. The worldmap voxel system (WorldMap::ProcessEffect())
	2. The ItemComponent (MOB) system (ItemComponent::ProcessEffect())

	Rules:
	1. map can catch MOB and map on fire
	2. MOB can catch map on fire
	3. MOB does NOT catch MOB on fire (too surprising in gameplay)
*/
void WorldMap::ProcessEffect(ChitBag* chitBag, int delta)
{
	PROFILE_FUNC();
//	QuickProfile("ProcessEffect");
	// We need process at a steady rate so that
	// the time between ticks is constant.
	// This is performance regressive, so something
	// to keep an eye on.
	static const int MAP2 = MAX_MAP_SIZE*MAX_MAP_SIZE;
	static const int DELTA = 1500;	// How frequenty to tick a given plant
	static const int N_PER_MSEC = MAP2 / DELTA;
	const float DAMAGE = EFFECT_DAMAGE_PER_SEC * float(DELTA) / float(1000);

	int nGrid = N_PER_MSEC * delta;
	effectCache.Clear();

	// Threaded: 27ms
	// Single:   50ms
#if defined(WORLDMAP_THREADS)
	{
		// Carefully spread out the threads to be as far apart
		// in memory as possible. Try to give the processor
		// cache some room.
		const static int QUARTER = MAP2 / ThreadPool::NTHREAD;
		int n = nGrid / ThreadPool::NTHREAD;
		GLASSERT(processIndex < QUARTER);
		n = Min(n, QUARTER - processIndex);

		ScanEffectsData data[ThreadPool::NTHREAD];

		for (int i = 0; i < ThreadPool::NTHREAD; ++i) {
			subEffectCache[i].Clear();
			data[i].random.SetSeed(random.Rand() + i*1000);
			data[i].effects = &subEffectCache[i];
			data[i].start = processIndex + i*QUARTER;
			data[i].n = n;
			data[i].worldMap = this;

			// Fire off 3 threads, use the main thread for the
			// end. There are only 4 threads on my system anyway,
			// no point having the main thread wait.
			if (i != ThreadPool::NTHREAD - 1)
				threadPool.Add(WorldMap::ScanEffects, (void*)&data[i]);
			else
				ScanEffects((void*)&data[i], nullptr, nullptr, nullptr);
		}
		threadPool.Wait(0);
		for (int i = 0; i < ThreadPool::NTHREAD; ++i) {
			for (int k = 0; k < subEffectCache[i].Size(); ++k) {
				effectCache.Push(subEffectCache[i][k]);
			}
		}
		processIndex += n;
		if (processIndex >= QUARTER) processIndex = 0;
	}
#else
	ScanEffectsData data;
	data.random.SetSeed(random.Rand());
	data.effects = &effectCache;
	data.start = processIndex;
	data.n = nGrid;
	data.worldMap = this;

	ScanEffects(&data);
	processIndex = (processIndex + nGrid) % MAP2;
#endif

	for (int i = 0; i < effectCache.Size(); ++i) {
		const int effect = effectCache[i].effect;
		const Vector2I pos2i = effectCache[i].pos;

		// Hit the voxel we are on:
		{
			DamageDesc dd(DAMAGE, effect);
			const Vector3I pos3i = { pos2i.x, 0, pos2i.y };
			this->VoxelHit(pos3i, dd);
		}

		// Finally, spread!
		static const int N = 8;
		static const Vector2I delta[N] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } };

		for (int j = 0; j < N; ++j) {
			Vector2I v2 = pos2i + delta[j];
			Vector3I v3 = { v2.x, 0, v2.y };
			int ia = INDEX(v2);
			if (grid[ia].Plant()) {
				if (random.Uniform() < CHANCE_FIRE_SPREAD) {
					DamageDesc dd(0, effect);
					this->VoxelHit(v3, dd);
				}
			}
		}
		// Make sure MOBs look around to ProcessEffects().
		// They aren't damaged directly, they scan the area 
		// around them.
		Vector2F origin = ToWorld2F(pos2i);
		chitBag->SetTickNeeded(origin, EFFECT_RADIUS);
	}
}


void WorldMap::FluidStats(PhysicsSims* context, int* pools, int* waterfalls)
{
	*pools = 0;
	*waterfalls = 0;
	for (int j = 0; j < NUM_SECTORS; ++j) {
		for (int i = 0; i < NUM_SECTORS; ++i) {
			Vector2I sector = { i, j };
			FluidSim* fluidSim = context->GetFluidSim(sector);
			if (fluidSim) {
				*pools += fluidSim->NumPools();
				*waterfalls += fluidSim->NumWaterfalls();
			}
		}
	}
}


void WorldMap::DoTick(U32 delta, ChitBag* chitBag)
{
	ProcessEffect(chitBag, delta);

	slowTick -= (int)(delta);

	// Send fire damage events, if needed.
	if (slowTick <= 0) {
		slowTick = SLOW_TICK;

		for (int i = 0; i < magmaGrids.Size(); ++i) {
			Vector2F origin = ToWorld2F(magmaGrids[i]);
			chitBag->SetTickNeeded(origin, EFFECT_RADIUS);
		}
	}

	// Do particles every time.
	//	ParticleDef pdEmber = engine->particleSystem->GetPD( "embers" );
	ParticleDef pdSmoke = engine->particleSystem->GetPD(ISC::smoke);
	//ParticleDef pdFire = engine->particleSystem->GetPD(ISC::fire);
	//ParticleDef pdShock = engine->particleSystem->GetPD(ISC::shock);

	for (int i = 0; i < magmaGrids.Size(); ++i) {
		Rectangle3F r;
		r.min.Set((float)magmaGrids[i].x, 0, (float)magmaGrids[i].y);
		r.max = r.min;
		r.max.x += 1.0f; r.max.z += 1.0f;

		int index = INDEX(magmaGrids[i]);
		if (grid[index].IsWater() || grid[index].IsFluid()) {
			r.min.y = r.max.y = grid[index].FluidHeight();
			engine->particleSystem->EmitPD(pdSmoke, r, V3F_UP, delta);
		}
		else {
			r.min.y = r.max.y = (float)grid[index].RockHeight();
			engine->particleSystem->EmitPD(pdSmoke, r, V3F_UP, delta);
		}
	}
	/*
	for (int i = 0; i < plantEffect.Size(); ++i) {
		Vector3F v = ToWorld3F(plantEffect[i].voxel);
		if (plantEffect[i].fire) {
			engine->particleSystem->EmitPD(pdSmoke, v, V3F_UP, delta);
			//engine->particleSystem->EmitPD(pdEmber, v, V3F_UP, delta);
			engine->particleSystem->EmitPD(pdFire, v, V3F_UP, delta);
		}
		if (plantEffect[i].shock) {
			engine->particleSystem->EmitPD(pdShock, v, V3F_UP, delta);
		}
	}
	*/
}


void WorldMap::SetPlant(int x, int y, int typeBase1, int stage)
{
	int index = INDEX(x, y);
	const WorldGrid was = grid[index];
	GLASSERT(typeBase1 == 0 || was.IsLand());

	WorldGrid wg = grid[index];
	wg.SetPlant(typeBase1, stage);
	wg.DeltaHP(wg.TotalHP());

	if (was.IsPassable() != wg.IsPassable()) {
		ResetPather(x, y);
	}
	grid[index] = wg;

	if (was.Plant()) {
		plantCount[was.Plant() - 1][was.PlantStage()] -= 1;
	}
	if (wg.Plant()) {
		plantCount[wg.Plant() - 1][wg.PlantStage()] += 1;
	}
}


void WorldMap::SetRock( int x, int y, int h, bool magma, int rockType )
{
	Vector2I vec	= { x, y };
	int index		= INDEX(x,y);
	const WorldGrid was = grid[index];

	if ( !was.IsLand() ) {
		return;
	}

	if ( h == -1 ) {
		// Set to default.
		if (    ( iMapGridUse && !iMapGridUse->MapGridBlocked( x, y ))
			 || ( !iMapGridUse ) )
		{
			h = grid[index].NominalRockHeight();
		}
		else {
			h = was.RockHeight();
		}
	}
	else if ( h == -2 ) {
		// Loading!
		if (was.Magma()) {
			GLASSERT(magmaGrids.Find(vec) < 0);
			magmaGrids.Push(vec);
		}
		h     = grid[index].RockHeight();
		if ( iMapGridUse ) {
			GLASSERT( iMapGridUse->MapGridBlocked( x, y ) == 0 );
		}
	}
	else {
		if ( iMapGridUse && iMapGridUse->MapGridBlocked( x, y )) {
			h = 0;
		}
	}
	WorldGrid wg = was;
	wg.SetRockHeight( h );
	wg.SetMagma( magma );
	wg.SetRockType( rockType );
	wg.DeltaHP( wg.TotalHP() );	// always repair. Correct?

	if (!was.VoxelEqual(wg)) {
		grid[INDEX(x, y)] = wg;

		if (physics) {
			Vector2I sector = ToSector(x, y);
			FluidSim* fluidSim = physics->GetFluidSim(sector);
			if (fluidSim) {
				fluidSim->Unsettle();
			}
		}
	}
	if ( was.IsPassable() != wg.IsPassable() ) {
		ResetPather( x, y );
	}
	if (was.Magma() != wg.Magma()) {

		if (wg.Magma()) {
			GLASSERT(magmaGrids.Find(vec) < 0);
			magmaGrids.Push(vec);
		}
		else {
			int i = magmaGrids.Find(vec);
			GLASSERT(i >= 0);
			if (i >= 0) {
				magmaGrids.SwapRemove(i);
			}
		}
	}
}


void WorldMap::UpdateBlock(const Rectangle2I& r)
{
	for (Rectangle2IIterator it(r); !it.Done(); it.Next()) {
		UpdateBlock(it.Pos().x, it.Pos().y);
	}
}

void WorldMap::UpdateBlock( int x, int y )
{
	WorldGrid* wg = &grid[INDEX(x,y)];
	bool blocked = false;
	if ( iMapGridUse ) {
		blocked = iMapGridUse->MapGridBlocked( x, y );
	}
	if (blocked && !wg->extBlock) {
		wg->extBlock = 1;
		ResetPather(x, y);
	}
	else if (!blocked && wg->extBlock) {
		wg->extBlock = 0;
		ResetPather(x, y);
	}
}


void WorldMap::GetWorldGrid(const grinliz::Vector2I&p, WorldGrid* arr, int count, Vector2I* dirArr)
{
	int step = 1;
	int n = 0;

	switch (count) {
		case 4: step = 2; break;
		case 5: step = 2; break;
		case 8: step = 1; break;
		case 9: step = 1; break;
		default: GLASSERT(0); break;
	}

	// Zero entry:
	if (count & 1) {
		arr[n] = grid[INDEX(p)];
		if (dirArr)
			dirArr[n].Zero();
		n++;
	}

	Rectangle2I bounds = Bounds();

	// Adjacent:
	for (int i = 0; i < 8; i += step) {
		const Vector2I v = p + DIR_I8[i];
		if (bounds.Contains(v)) {
			arr[n] = grid[INDEX(v)];
		}
		else {
			// return water if out of bounds. much
			// simpler than a bunch of upstream checks.
			memset(&arr[n], 0, sizeof(WorldGrid));
		}
		if (dirArr)
			dirArr[n] = DIR_I8[i];
		n++;
	}
}


void WorldMap::ResetPather( int x, int y )
{
	Vector2I sector = ToSector(x, y);
	micropather::MicroPather* pather = GetPather(sector, false);
	if (pather)
		pather->Reset();
	zoneInit.Clear( x>>ZONE_SHIFT, y>>ZONE_SHIFT);
}


bool WorldMap::IsPassable( int x, int y, bool ignoreBuildings ) const
{
	int index = INDEX(x,y);
	const WorldGrid& wg = grid[index];
	return ignoreBuildings ? wg.IsInternalPassable() : wg.IsPassable();
}


Vector2I WorldMap::FindPassable( int x, int y )
{
	int c = 0;

	while ( true ) {
		int x0 = Max( 0, x-c );
		int x1 = Min( width-1, x+c );
		int y0 = Max( 0, y-c );
		int y1 = Min( height-1, y+c );

		for( int j=y0; j<=y1; ++j ) {
			for( int i=x0; i<=x1; ++i ) {
				if ( j==y0 || j==y1 || i==x0 || i==x1 ) {
					if ( IsPassable(i,j) ) {
						Vector2I v = { i, j };
						return v;
					}
				}
			}
		}
		++c;
	}
	GLASSERT( 0 );	// The world is full?
	Vector2I vz = { 0, 0 };
	return vz;
}


WorldMap::BlockResult WorldMap::CalcBlockEffect(	const grinliz::Vector2F& pos,
													float rad,
													BlockType type,
													grinliz::Vector2F* force )
{
	Vector2I pos2i = ToWorld2I(pos);
	GLASSERT(rad <= MAX_BASE_RADIUS);

	Rectangle2I bounds = Bounds();
	static const float EPSILON = 0.01f;

	force->Zero();
	float smallest = 1.0f;

	static const Vector2F origin = { 0.5f, 0.5f };
	double d0, d1;
	const Vector2F fract = { (float)modf(pos.x, &d0), (float)modf(pos.y, &d1) };

	for (int i = 0; i < 8; ++i) {

		Vector2F v = pos + DIR_F8[i] * rad;
		Vector2I block = ToWorld2I(v);

		if (block == pos2i) continue;	// same block

		bool blocked = !bounds.Contains(block);
		if (!blocked) {
			if (type == BT_PASSABLE)   blocked = !IsPassable(block.x, block.y);
			else if (type == BT_FLUID) blocked = !grid[INDEX(block)].IsFluid() && !IsPassable(block.x, block.y);
		}
		if (!blocked) continue;

		// Treat block as infinite plane. Calc distance to plane,
		// projection of delta onto normal.
		Vector2F corner = origin + 0.5f * DIR_F8[i];		// the corner, in local (0,0)-(1,1) coordinates.
		Vector2F normal = DIR_F8[i] * INV_LEN_F8[i];		// normal in the direction
		float d = DotProduct(normal, corner - fract);		// distance projection, treating the block as a plane. corner blocks are 45deg planes.

		Vector2F f = { 0, 0 };
		if (d < rad) {
			f = -normal * (rad - d + EPSILON);

			if (f.LengthSquared() < smallest) {
				smallest = f.LengthSquared();
				*force = f;
			}
		}
	}
	return force->IsZero() ? NO_EFFECT : FORCE_APPLIED;
}


WorldMap::BlockResult WorldMap::ApplyBlockEffect(	const Vector2F inPos, 
													float radius, 
													BlockType type,
													Vector2F* outPos )
{
	GLASSERT(BoundsF().Contains(inPos));
	*outPos = inPos;

	// If blocked on input, no fixing that:
	Vector2I pos2i = ToWorld2I(inPos);
	if (grid[INDEX(pos2i)].IsBlocked()) {
		*outPos = ToWorld2F(FindPassable(pos2i.x, pos2i.y));
		return FORCE_APPLIED;
	}

	Vector2F force = { 0, 0 };

	// Can't think of a case where it's possible to overlap more than 2,
	// but there probably is. Don't worry about it. Go with fast & 
	// usually good enough.
	//for( int i=0; i<2; ++i ) {
	BlockResult result = CalcBlockEffect( *outPos, radius, type, &force );
	*outPos += force;	
	//}

	pos2i = ToWorld2I(*outPos);
	if (grid[INDEX(pos2i)].IsBlocked()) {
		GLASSERT(false);	// shouldn't happen - we weren't blocked at start of function.
		*outPos = ToWorld2F(FindPassable(pos2i.x, pos2i.y));
	}
	GLASSERT(BoundsF().Contains(*outPos));

	// This seems like a good idea, but leads to cases where
	// the in == out and the pather is stuck. 
	//return ( *outPos == inPos ) ? NO_EFFECT : FORCE_APPLIED;
	return result;
}


void WorldMap::CalcZone( int mapX, int mapY )
{
	struct SZ {
		U8 x;
		U8 y;
		U8 size;
	};

	int zx = mapX & (~(ZONE_SIZE-1));
	int zy = mapY & (~(ZONE_SIZE-1));

	if ( !zoneInit.IsSet( zx>>ZONE_SHIFT, zy>>ZONE_SHIFT) ) {
		int mask[ZONE_SIZE];
		memset( mask, 0, sizeof(int)*ZONE_SIZE );
		CArray< SZ, ZONE_SIZE*ZONE_SIZE > stack;

		//GLOUTPUT(( "CalcZone (%d,%d) %d\n", zx, zy, ZDEX(zx,zy) ));
		zoneInit.Set( zx>>ZONE_SHIFT, zy>>ZONE_SHIFT);

		// Build up a bit pattern to analyze.
		for( int y=0; y<ZONE_SIZE; ++y ) {
			for( int x=0; x<ZONE_SIZE; ++x ) {
				if ( IsPassable(zx+x,zy+y) ) {
					mask[y] |= 1 << x;
				}
				else {
					grid[INDEX(zx+x, zy+y)].SetZoneSize(0);
				}
			}
		}

		SZ start = { 0, 0, ZONE_SIZE };
		stack.Push( start );

		while( !stack.Empty() ) {
			const SZ sz = stack.Pop();
			
			// So we can quickly check for everything set,
			// create a mask to iterate over the bitf
			int m = (1<<sz.size)-1;
			m <<= sz.x;

			int j=sz.y;
			for( ; j<sz.y+sz.size; ++j ) {
				if ( (mask[j] & m) != m )
					break;
			}

			if ( j == sz.y + sz.size ) {
				// Match.
				for( int y=sz.y; y<sz.y+sz.size; ++y ) {
					for( int x=sz.x; x<sz.x+sz.size; ++x ) {
						grid[INDEX(zx+x, zy+y)].SetZoneSize(sz.size);
					}
				}
			}
			else if ( sz.size > 1 ) {
				SZ sz00 = { U8(sz.x), U8(sz.y), U8(sz.size/2) };
				SZ sz10 = { U8(sz.x+sz.size/2), U8(sz.y), U8(sz.size/2) };
				SZ sz01 = { U8(sz.x), U8(sz.y+sz.size/2), U8(sz.size/2) };
				SZ sz11 = { U8(sz.x+sz.size/2), U8(sz.y+sz.size/2), U8(sz.size/2) };
				stack.Push( sz00 );
				stack.Push( sz10 );
				stack.Push( sz01 );
				stack.Push( sz11 );
			}
		}
	}
}


// micropather
float WorldMap::LeastCostEstimate( void* stateStart, void* stateEnd )
{
	Vector2I startI, endI;
	ToGrid( stateStart, &startI );
	ToGrid( stateEnd, &endI );

	Vector2F start = ZoneCenter( startI.x, startI.y );
	Vector2F end   = ZoneCenter( endI.x, endI.y );
	return (end-start).Length();
}


// micropather
void WorldMap::AdjacentCost( void* state, MP_VECTOR< micropather::StateCost > *adjacent )
{
	Vector2I start;
	const WorldGrid* startGrid = ToGrid( state, &start );

	GLASSERT( IsPassable( start.x, start.y ));
	Vector2F startC = ZoneCenter( start.x, start.y );

	Rectangle2I mapBounds = this->Bounds();
	int size = startGrid->ZoneSize();
	GLASSERT( size > 0 );

	Rectangle2I zoneBounds;
	zoneBounds.Set( start.x, start.y, start.x+size-1, start.y+size-1 );
	Rectangle2I adjBounds = zoneBounds;
	adjBounds.Outset( 1 );

	CArray< Vector2I, ZONE_SIZE*4+4 > adj;			// origins

	Rectangle2IEdgeIterator it( adjBounds );
	Vector2I currentOrigin = { -1, -1 };

	for( it.Begin(); !it.Done(); it.Next() ) {
		Vector2I v = it.Pos();
		if ( !mapBounds.Contains( v )) {
			continue;
		}
		// Move this down? But be careful to flush
		// everything.
		CalcZone( v.x, v.y );
		Vector2I origin = ZoneOrigin( v.x, v.y );
		if ( origin == currentOrigin ) {
			// just looked at this.
			continue;
		}
		if ( adj.Find( origin ) < 0 ) {
			adj.Push( origin );
			currentOrigin = origin;
		}
	}

	for( int i=0; i<adj.Size(); ++i ) {
		Vector2I origin = adj[i];	
		const WorldGrid* wg = ZoneOriginG( origin.x, origin.y );

		if ( wg->ZoneSize() && IsPassable( origin.x, origin.y )) {
			// We can path to it - unless it's a corner. In
			// which case extra checks are needed
			Rectangle2I b;
			b.Set( origin.x, origin.y, origin.x+wg->ZoneSize()-1, origin.y+wg->ZoneSize()-1 );

			int dx = 0, dy = 0;
			int cx = 0, cy = 0;
			if ( b.max.x < zoneBounds.min.x ) { dx = -1; cx = zoneBounds.min.x; }
			if ( b.min.x > zoneBounds.max.x ) { dx = 1;  cx = zoneBounds.max.x; }
			if ( b.max.y < zoneBounds.min.y ) { dy = -1; cy = zoneBounds.min.y; }
			if ( b.min.y > zoneBounds.max.y ) { dy = 1;  cy = zoneBounds.max.y; }

			bool okay = false;
			if ( abs(dx) == 1 && abs(dy) == 1 )
			{
				// *sigh* corner.
				// We know the corner itself is passable, check neighbors:
				if ( IsPassable( cx+dx, cy ) && IsPassable( cx, cy+dy )) {
					okay = true;
				}
			}
			else {
				okay = true;	// not a corner - already checked.
			}

			if ( okay ) {
				Vector2F otherC = ZoneCenter( origin.x, origin.y );
				float cost = (otherC-startC).Length();
				void* s = ToState( origin.x, origin.y );
				GLASSERT( s != state );
				micropather::StateCost sc = { s, cost };
				adjacent->push_back( sc );
			}
		}
	}
}


// micropather
void WorldMap::PrintStateInfo( void* state )
{
	Vector2I vec;
	WorldGrid* g = ToGrid( state, &vec );
	int size = g->ZoneSize();
	GLOUTPUT(( "(%d,%d)s=%d ", vec.x, vec.y, size ));
	(void)size;
}

int WorldMap::IntersectPlantAtVoxel(const grinliz::Vector3I& voxel,
									const grinliz::Vector3F& origin, 
									const grinliz::Vector3F& dir, 
									float length, 
									grinliz::Vector3F* at)
{
	int index = INDEX(voxel.x, voxel.z);
	const WorldGrid& wg = grid[index];
	if (!wg.Plant()) {
		return REJECT;
	}

	// We only need the one tree. This isn't a rendering pass,
	// so the nTrees doesn't matter.
	nTrees = 0;
	// Create the tree we need for testing.
	Model* m = PushTree(voxel.x, voxel.z, wg.Plant() - 1, wg.PlantStage(), 1.0f);

	int result = m->IntersectRay(true, origin, dir, at);
	if (result == INTERSECT) {
		float lenToImpact = (*at - origin).Length();
		if (lenToImpact <= length) return INTERSECT;
	}
	return REJECT;
}


// The paper: "A Fast Voxel Traversal Algorithm for Ray Tracing" by Amanatides and Woo
// based on code at: http://www.xnawiki.com/index.php?title=Voxel_traversal

Vector3I WorldMap::IntersectVoxel(	const Vector3F& origin,
									const Vector3F& dir,
									float length,				
									Vector3F* at )
{
	GLASSERT( Equal( dir.Length(), 1.0f, 0.0001f ));
	GLASSERT( length >= 0 );
	static const Vector3I noResult = { -1, -1, -1 };

	Vector3F p0, p1;
	int test0, test1;
	Rectangle3F bounds;
	bounds.Set( 0, 0, 0, (float)width, (float)MAX_ROCK_HEIGHT, (float)height );
	Rectangle3I ibounds;
	ibounds.Set( 0, 0, 0, width-1, MAX_ROCK_HEIGHT-1, height-1 );

	// Where this comes in and out of the world voxels.
	IntersectRayAllAABB( origin, dir, bounds, &test0, &p0, &test1, &p1 );

	if ( test0 == REJECT || test1 == REJECT ) {
		// voxel grid isn't intersected.
		return noResult;
	}

	// Keep in mind floating point issues. Certainly can have p values
	// greater than the cell coordinates (y=3.0 rounds to 3) and probably
	// possible to get small negative as well.
	Vector3I startCell = { (int)p0.x, (int)p0.y, (int)p0.z };
	Vector3I endCell   = { (int)p1.x, (int)p1.y, (int)p1.z };

	Vector3I step = { (int)Sign(dir.x), (int)Sign(dir.y), (int)Sign(dir.z) };
	Vector3F boundary = { (float)startCell.x + ((step.x>0) ? 1.f : 0),
						  (float)startCell.y + ((step.y>0) ? 1.f : 0),
						  (float)startCell.z + ((step.z>0) ? 1.f : 0) };

	// Distance, in t, to the next boundary. Being careful of straight lines.
	Vector3F tMax = { 0, 0, 0 };
	Vector3F tDelta = { 0, 0, 0 };

	int n = 0;
	for( int i=0; i<3; ++i ) {
		if ( dir.X(i) ) {
			tMax.X(i)   = (boundary.X(i) - p0.X(i)) / dir.X(i);	// how far before voxel boundary
			tDelta.X(i) = ((float)step.X(i)) / dir.X(i);		// how far to cross a voxel
			n += 1 + abs(startCell.X(i) - endCell.X(i));
			GLASSERT( tMax.X(i) >= 0 && tDelta.X(i) >= 0 );
		}
	}

	Vector3I cell = startCell;
	int lastStep = -1;

	for( int i=0; i<n; ++i ) {

		// The only part specific to the map.
		if ( ibounds.Contains( cell )) {
			const WorldGrid& wg = this->GetWorldGrid( cell.x, cell.z );

			if (wg.Plant()) {
				if (IntersectPlantAtVoxel(cell, origin, dir, length, at) == INTERSECT) {
					return cell;
				}
			}
			if ( cell.y < wg.Height()) 
			{
				Vector3F v;
				if ( lastStep < 0 ) {
					v = origin;
				}
				else {
					// The ray always hits the near wall.
					float x = (dir.X(lastStep) > 0 ) ? (float)cell.X(lastStep) : (float)cell.X(lastStep)+1;
					IntersectRayPlane( origin, dir, lastStep, x, &v );
				}
				if ( (v-origin).LengthSquared() <= length*length ) {
					if ( at ) {
						*at = v;
					}
					return cell;
				}
				return noResult;
			}
		}

		if ( tMax.x < tMax.y && tMax.x < tMax.z ) {
			cell.x += step.x;
			tMax.x += tDelta.x;
			lastStep = 0;
		}
		else if ( tMax.y < tMax.z ) {
			cell.y += step.y;
			tMax.y += tDelta.y;
			lastStep = 1;
		}
		else {
			cell.z += step.z;
			tMax.z += tDelta.z;
			lastStep = 2;
		}
	}

	return noResult;
}


// Such a good site, for many years: http://www-cs-students.stanford.edu/~amitp/gameprog.html
// Specifically this link: http://playtechs.blogspot.com/2007/03/raytracing-on-grid.html
// Returns true if there is a straight line path between the start and end.
// The line-walk can/should get moved to the utility package. (and the grid lookup replaced with visit() )
bool WorldMap::GridPath( const grinliz::Vector2F& p0, const grinliz::Vector2F& p1, bool ignoreBuildings )
{
	if (ToWorld2I(p0) == ToWorld2I(p1))
		return true;	// start-end same

	double dx = fabs(p1.x - p0.x);
    double dy = fabs(p1.y - p0.y);

    int x = int(floor(p0.x));
    int y = int(floor(p0.y));

    int n = 1;
    int x_inc, y_inc;
    double error;

    if (p1.x > p0.x) {
        x_inc = 1;
        n += int(floor(p1.x)) - x;
        error = (floor(p0.x) + 1 - p0.x) * dy;
    }
    else {
        x_inc = -1;
        n += x - int(floor(p1.x));
        error = (p0.x - floor(p0.x)) * dy;
    }

    if (p1.y > p0.y) {
        y_inc = 1;
        n += int(floor(p1.y)) - y;
        error -= (floor(p0.y) + 1 - p0.y) * dx;
    }
    else {
        y_inc = -1;
        n += y - int(floor(p1.y));
        error -= (p0.y - floor(p0.y)) * dx;
    }

    for (; n > 0; --n) {
		CalcZone( x,y );

        if ( !IsPassable(x, y, ignoreBuildings) )
			return false;

        if (error > 0) {
            y += y_inc;
            error -= dx;
        }
        else {
            x += x_inc;
            error += dy;
        }
    }
	return true;
}


SectorPort WorldMap::RandomPort( grinliz::Random* random )
{
	if ( randomPortDebug.IsValid() ) return randomPortDebug;

	SectorPort sp;
	while ( true ) {
		Vector2I sector = { int(random->Rand( NUM_SECTORS )), int(random->Rand( NUM_SECTORS )) };
		const SectorData& sd = GetSectorData( sector );
		if ( sd.HasCore() ) {
			GLASSERT( sd.ports );
			sp.sector = sd.sector;
			for( int i=0; i<4; ++i ) {
				int port = 1 << i;
				if ( sd.ports & port ) {
					sp.port = port;
					break;
				}
			}
			break;
		}
	}
	return sp;
}


SectorPort WorldMap::NearestPort(const Vector2F& pos, const Vector2F* origin)
{
	Vector2I secPos = ToSector(pos);
	const SectorData& sd = worldInfo->GetSectorData(secPos);

	int   bestPort = 0;
	float bestCost = FLT_MAX;

	// The first loop finds the best port assuming we want to go to 'pos'.
	// If that fails (which is unusual) drop back to finding the nearest port.
	for (int i = 0; i < 4; ++i) {
		int port = (1 << i);
		if (sd.ports & port) {
			Vector2I desti = sd.GetPortLoc(port).Center();
			Vector2F dest = { (float)desti.x, (float)desti.y };
			float cost = FLT_MAX;

			if (CalcPath(pos, dest, 0, &cost, false)) {
				if (origin) {
					// Bias towards the side nearest the origin.
					// Else tends to look a little weird.
					cost += (*origin - pos).Length() * 0.25f;
				}

				if (cost < bestCost) {
					bestCost = cost;
					bestPort = port;
				}
			}
		}
	}
	if (!bestPort) {
		for (int i = 0; i < 4; ++i) {
			int port = (1 << i);
			if (sd.ports & port) {
				Vector2I desti = sd.GetPortLoc(port).Center();
				Vector2F dest = { (float)desti.x, (float)desti.y };
				float cost = (pos - dest).Length();
				if (cost < bestCost) {
					bestCost = cost;
					bestPort = port;
				}
			}
		}
	}

	SectorPort result;
	if (bestPort) {
		result.port = bestPort;
		result.sector = secPos;
	}
	return result;
}


bool WorldMap::TeleportToSector(const Vector2I& sector, Chit* chit)
{
	Rectangle2I inner = InnerSectorBounds(sector);
	for (int r = 0; r < INNER_SECTOR_SIZE; ++r) {
		Rectangle2I edge;
		edge.Set(inner.Center().x - r, inner.Center().y - r, inner.Center().x + 1 + r, inner.Center().x + 1 + r);
		for (Rectangle2IEdgeIterator edgeIt(edge); !edgeIt.Done(); edgeIt.Next()) {
			if (this->IsPassable(edgeIt.Pos().x, edgeIt.Pos().y)) {
				SpatialComponent::Teleport(chit, ToWorld3F(edgeIt.Pos()));
				return true;
			}
		}
	}
	return false;
}

bool WorldMap::HasStraightPath(const Vector2F& start, const Vector2F& end, bool ignoreBuildings)
{
	// Try a straight line ray cast
	return GridPath(start, end, ignoreBuildings);
}


bool WorldMap::HasStraightPathBeside(const Vector2F& start, const Rectangle2I& rect)
{
	if (start.x < rect.min.x) {
		for (int y = rect.min.y; y <= rect.max.y; ++y) {
			Vector2I end2i = { rect.min.x - 1, y };
			bool path = GridPath(start, ToWorld2F(end2i), false);
			if (path) return true;
		}
	}
	if (start.x > rect.max.x) {
		for (int y = rect.min.y; y <= rect.max.y; ++y) {
			Vector2I end2i = { rect.max.x + 1, y };
			bool path = GridPath(start, ToWorld2F(end2i), false);
			if (path) return true;
		}
	}
	if (start.y < rect.min.y) {
		for (int x = rect.min.x; x <= rect.max.x; ++x) {
			Vector2I end2i = { x, rect.min.y - 1 };
			GridPath(start, ToWorld2F(end2i), false);
		}
	}
	if (start.y > rect.max.y) {
		for (int x = rect.min.x; x <= rect.max.x; ++x) {
			Vector2I end2i = { x, rect.max.y + 1 };
			GridPath(start, ToWorld2F(end2i), false);
		}
	}
	return false;
}


micropather::MicroPather* WorldMap::GetPather(const grinliz::Vector2I& sector, bool createIfNeeded)
{
	GLASSERT( sector.x >= 0 && sector.x < NUM_SECTORS );
	GLASSERT( sector.y >= 0 && sector.y < NUM_SECTORS );
	int index = sector.y * NUM_SECTORS + sector.x;

	if (!pathers[index] && createIfNeeded) {
		const SectorData& sd = this->GetSectorData(sector);
		pathers[index] = new micropather::MicroPather( this, sd.area ? sd.area : 1000, 7, true );
	}
	return pathers[index];
}


bool WorldMap::CalcWorkPath(const grinliz::Vector2F& start,
							const grinliz::Rectangle2I& end,
							grinliz::Vector2F* bestEnd,
							float* totalCost)
{
	Vector2I best = end.min;
	bool okay = CalcPath(start, ToWorld2F(best), 0, totalCost, false);
	if (okay) {
		*bestEnd = ToWorld2F(best);
		return true;
	}

	Rectangle2I r = end;
	r.Outset(1);

	for (Rectangle2IIterator it(r); !it.Done(); it.Next()) {
		if (it.Pos() != best) {
			okay = CalcPath(start, ToWorld2F(it.Pos()), 0, totalCost, false);
			if (okay) {
				*bestEnd = ToWorld2F(it.Pos());
				return true;
			}
		}
	}
	return false;
}


bool WorldMap::CalcPathBeside(	const grinliz::Vector2F& start, 
								const grinliz::Vector2F& end, 
								grinliz::Vector2F* bestEnd,
								float* totalCost )
{
	static const Vector2F delta[4] = { {-1,0}, {1,0}, {0,-1}, {0,1} };

	int best = -1;
	float bestCost = FLT_MAX;

	for( int i=0; i<4; ++i ) {
		float cost = 0;
		if ( CalcPath( start, end + delta[i], 0, &cost, false )) {
			if ( cost < bestCost ) {
				bestCost = cost;
				best = i;
			}
		}
	}
	if ( best >= 0 ) {
		*bestEnd = end + delta[best];
		*totalCost = bestCost;
		return true;
	}
	return false;
}


bool WorldMap::CalcPath(const grinliz::Vector2F& start,
						const grinliz::Vector2F& end,
						CDynArray<grinliz::Vector2F> *path,
						float *totalCost,
						bool debugging)
{
	debugPathVector.Clear();
	if (!path) {
		path = &pathCache;
	}
	path->Clear();
	bool okay = false;
	float dummyCost = 0;
	if (!totalCost) totalCost = &dummyCost;	// prevent crash later.

	Vector2I starti = { (int)start.x, (int)start.y };
	Vector2I endi = { (int)end.x, (int)end.y };
	Vector2I sector = ToSector(starti);

	// Flush out regions that aren't valid.
	// Don't do this. Use the AdjacentCost for this.
	//for( int j=0; j<height; j+= ZONE_SIZE ) {
	//	for( int i=0; i<width; i+=ZONE_SIZE ) {
	//		CalcZone( i, j );
	//	}
	//}

	// But do flush the current region.
	CalcZone(starti.x, starti.y);
	CalcZone(endi.x, endi.y);

	WorldGrid* wgStart = grid + INDEX(starti.x, starti.y);
	WorldGrid* wgEnd = grid + INDEX(endi.x, endi.y);

	if (!IsPassable(starti.x, starti.y) || !IsPassable(endi.x, endi.y)) {
		return false;
	}
	GLASSERT(wgStart->ZoneSize());	// persistant bug: if passable, should have zone. (was a typo in the CalcZone call.)
	GLASSERT(wgEnd->ZoneSize());	// persistant bug: if passable, should have zone.

	// Regions are convex. If in the same region, it is passable.
	if (wgStart->ZoneOrigin(starti.x, starti.y) == wgEnd->ZoneOrigin(endi.x, endi.y)) {
		okay = true;
		path->Push(start);
		path->Push(end);
		*totalCost = (end - start).Length();
	}

	// Try a straight line ray cast
	if (!okay) {
		okay = GridPath(start, end, false);
		if (okay) {
			path->Push(start);
			path->Push(end);
			*totalCost = (end - start).Length();
		}
	}

	// Use the region solver.
	if (!okay) {
		micropather::MicroPather* pather = GetPather(sector);

		int result = pather->Solve(ToState(starti.x, starti.y), ToState(endi.x, endi.y),
								   &pathRegions, totalCost);
		if (result == micropather::MicroPather::SOLVED) {
			//GLOUTPUT(( "Region succeeded len=%d.\n", pathRegions.size() ));
			Vector2F from = start;
			path->Push(start);
			okay = true;

			// Walk each of the regions, and connect them with vectors.
			for (unsigned i = 0; i < pathRegions.size() - 1; ++i) {
				Vector2I originA, originB;
				ToGrid(pathRegions[i], &originA);
				ToGrid(pathRegions[i + 1], &originB);

				Rectangle2F bA = ZoneBounds(originA.x, originA.y);
				Rectangle2F bB = ZoneBounds(originB.x, originB.y);
				bA.DoIntersection(bB);

				// Every point on a path needs to be obtainable,
				// else the chit will get stuck. There inset
				// away from the walls so we don't put points
				// too close to walls to get to.
				static const float INSET = MAX_BASE_RADIUS;
				if (bA.min.x + INSET*2.0f < bA.max.x) {
					bA.min.x += INSET;
					bA.max.x -= INSET;
				}
				if (bA.min.y + INSET*2.0f < bA.max.y) {
					bA.min.y += INSET;
					bA.max.y -= INSET;
				}

				Vector2F v = bA.min;
				if (bA.min != bA.max) {
					int result = ClosestPointOnLine(bA.min, bA.max, from, &v, true);
					GLASSERT(result == INTERSECT);
					if (result == REJECT) {
						okay = false;
						break;
					}
				}
				path->Push(v);
				from = v;
			}
			path->Push(end);
		}
	}

	if (okay) {
		if (debugging) {
			for (int i = 0; i < path->Size(); ++i)
				debugPathVector.Push((*path)[i]);
		}
	}
	else {
		path->Clear();
	}
	return okay;
}


void WorldMap::ClearDebugDrawing()
{
	for( int i=0; i<width*height; ++i ) {
		grid[i].SetDebugAdjacent( false );
		grid[i].SetDebugOrigin( false );
		grid[i].SetDebugPath( false );
	}
	debugPathVector.Clear();
}


void WorldMap::ShowRegionPath( float x0, float y0, float x1, float y1 )
{
	ClearDebugDrawing();

	void* start = ToState( (int)x0, (int)y0 );
	void* end   = ToState( (int)x1, (int)y1 );
	Vector2I sector = ToSector(x0, y0);
	
	if ( start && end ) {
		float cost=0;
		micropather::MicroPather* pather = GetPather( sector );
		int result = pather->Solve( start, end, &pathRegions, &cost );

		if ( result == micropather::MicroPather::SOLVED ) {
			for( unsigned i=0; i<pathRegions.size(); ++i ) {
				WorldGrid* vp = ToGrid( pathRegions[i], 0 );
				vp->SetDebugPath( true );
			}
		}
	}
}


void WorldMap::ShowAdjacentRegions( float fx, float fy )
{
	int x = (int)fx;
	int y = (int)fy;
	ClearDebugDrawing();

	if ( IsPassable( x, y ) ) {
		WorldGrid* r = ZoneOriginG( x, y );
		r->SetDebugOrigin( true );

		MP_VECTOR< micropather::StateCost > adj;
		AdjacentCost( ToState( x, y ), &adj );
		for( unsigned i=0; i<adj.size(); ++i ) {
			WorldGrid* n = ToGrid( adj[i].state, 0 );
			GLASSERT( n->DebugAdjacent() == false );
			n->SetDebugAdjacent( true );
		}
	}
}


void WorldMap::DrawTreeZones()
{
	CompositingShader debug( BLEND_NORMAL );
	debug.SetColor( 0.2f, 0.8f, 0.6f, 0.5f );
	if ( engine ) {
		const CArray<Rectangle2I, SpaceTree::MAX_ZONES>& zones = engine->GetSpaceTree()->Zones();

		for( int i=0; i<zones.Size(); ++i ) {
			Rectangle2I r = zones[i];
			Vector3F p0 = { (float)r.min.x, 0.05f, (float)r.min.y };
			Vector3F p1 = { (float)r.max.x+0.95f, 0.05f, (float)r.max.y+0.95f };
			GPUDevice::Instance()->DrawQuad( debug, 0, p0, p1, false );
		}
	}
}


void WorldMap::DrawZones()
{
	CompositingShader debug( BLEND_NORMAL );
	debug.SetColor( 1, 1, 1, 0.5f );
	CompositingShader debugOrigin( BLEND_NORMAL );
	debugOrigin.SetColor( 1, 0, 0, 0.5f );
	CompositingShader debugAdjacent( BLEND_NORMAL );
	debugAdjacent.SetColor( 1, 1, 0, 0.5f );
	CompositingShader debugPath( BLEND_NORMAL );
	debugPath.SetColor( 0.5f, 0.5f, 1, 0.5f );

	GPUDevice* device = GPUDevice::Instance();

	for( int j=debugRegionOverlay.min.y; j<=debugRegionOverlay.max.y; ++j ) {
		for( int i=debugRegionOverlay.min.x; i<=debugRegionOverlay.max.x; ++i ) {
			CalcZone( i, j );

			static const float offset = 0.1f;
			
			if ( IsZoneOrigin( i, j ) ) {
				if ( IsPassable( i, j ) ) {

					const WorldGrid& gs = grid[INDEX(i,j)];
					Vector3F p0 = { (float)i+offset, 0.01f, (float)j+offset };
					Vector3F p1 = { (float)(i+gs.ZoneSize())-offset, 0.01f, (float)(j+gs.ZoneSize())-offset };

					if ( gs.DebugOrigin() ) {
						device->DrawQuad( debugOrigin, 0, p0, p1, false );
					}
					else if ( gs.DebugAdjacent() ) {
						device->DrawQuad( debugAdjacent, 0, p0, p1, false );
					}
					else if ( gs.DebugPath() ) {
						device->DrawQuad( debugPath, 0, p0, p1, false );
					}
					else {
						device->DrawQuad( debug, 0, p0, p1, false );
					}
				}
			}
		}
	}
}


void WorldMap::CreateTexture( Texture* t )
{
	if ( StrEqual( t->Name(), "miniMap" ) ) {
		
		int size = t->Width()*t->Height();
		U16* data = new U16[size];

		int dx = this->Width() / t->Width();
		int dy = this->Height() / t->Height();

		const Game::Palette* palette = Game::GetMainPalette();
		Color4U8 cWater = palette->Get4U8( PAL_BLUE*2, PAL_BLUE );
		Color4U8 cGrid  = palette->Get4U8( PAL_PURPLE*2, PAL_PURPLE );
		Color4U8 cPort  = palette->Get4U8( PAL_TANGERINE*2, PAL_TANGERINE );
		Color4U8 cLand  = palette->Get4U8( PAL_PURPLE*2, PAL_GREEN );

		U16 c[WorldGrid::NUM_LAYERS] = {
			Surface::CalcRGB16( cWater ),
			Surface::CalcRGB16( cLand ),
			Surface::CalcRGB16( cGrid ),
			Surface::CalcRGB16( cPort ),
		};

		for( int y=0; y<t->Height(); ++y ) {
			for( int x=0; x<t->Width(); ++x ) {
				const WorldGrid& wg = grid[INDEX(x*dx,y*dy)];
				int index = y * t->Width() + x;
				GLASSERT(index < size);
				if (index < size) {
					data[index] = c[wg.Land()];
				}
			}
		}

		t->Upload( data, sizeof(U16)*size );
		delete [] data;
	}
	else {
		GLASSERT( 0 );
	}
}


void WorldMap::Submit( GPUState* shader )
{
	if ( !gridTexture ) {
		gridTexture = TextureManager::Instance()->GetTexture( "voxmap" );
	}
	
	Vertex v;
	GPUStream stream( v );

	GPUStreamData data;
	data.vertexBuffer	= gridVertexVBO->ID();
	data.texture0		= gridTexture;
	// no fix, saturation, emmissive
	//Vector4F control	= { 1, Saturation()*GRID_SATURATION, 1, 1 };
	GPUControlParam	control;
	control.saturation = Saturation()*GRID_SATURATION;
	data.controlParam	= &control;

	GPUDevice::Instance()->DrawQuads( *shader, stream, data, nGrids );
}


float WorldMap::IndexToRotation360(int index)
{
	static const float MULT = 360.0f / 255.0f;
	return float(Random::Hash8(index)) * MULT;
}


Model* WorldMap::PushTree(int x, int y, int type0Based, int stage, float hpFraction)
{
	GLASSERT(type0Based >= 0 && type0Based < NUM_EXTENDED_PLANT_TYPES);
	GLASSERT(stage >= 0 && stage < 4);

	const ModelResource* res = PlantScript::PlantRes( type0Based, stage );
	GLASSERT(res);

	if (treePool.Size() == nTrees) {
		treePool.Push(new Model(res, engine->GetSpaceTree()));
	}
	Model* m = treePool[nTrees];
	m->ClearFlag(Model::MODEL_INVISIBLE);
	nTrees++;

	m->SetResource(res);
	m->SetFlag(Model::MODEL_CLICK_THROUGH);
	Vector3F pos = { float(x) + 0.5f, 0, float(y) + 0.5f };
	float rot = IndexToRotation360(INDEX(x, y));
	m->SetPosAndYRotation(pos, rot);
	m->SetSaturation(hpFraction * PLANT_SATURATION);

	return m;
}


void WorldMap::PrepGrid( const SpaceTree* spaceTree )
{
	// For each region of the spaceTree that is visible,
	// generate voxels.
	if ( !gridVertexVBO ) {
		gridVertexVBO = new GPUVertexBuffer( 0, sizeof(Vertex)*voxelBuffer.Capacity() );
	}
	voxelBuffer.Clear();

	// HARDCODE black magic values.
	static const float du = 0.250;
	static const float dv = 0.125;
	#define BLACKMAG_X(x)	float( double(x*288 + 16) / 1024.0)
	#define BLACKMAG_Y(y)   float( 0.875 - double(y*288 + 16) / 2048.0)

	static const Vector2F UV[] = {
		{ BLACKMAG_X(1), BLACKMAG_Y(0) },	// water
		{ BLACKMAG_X(0), BLACKMAG_Y(0) },	// land
		{ BLACKMAG_X(0), BLACKMAG_Y(1) },	// grid
		{ BLACKMAG_X(1), BLACKMAG_Y(1) },	// port
		{ BLACKMAG_X(0), BLACKMAG_Y(2) },	// pave1
		{ BLACKMAG_X(2), BLACKMAG_Y(0) },	// pave2
		{ BLACKMAG_X(2), BLACKMAG_Y(1) },	// pave3
		{ BLACKMAG_X(2), BLACKMAG_Y(2) },	// porch
		{ BLACKMAG_X(1), BLACKMAG_Y(4) },	// porch --
		{ BLACKMAG_X(0), BLACKMAG_Y(4) },	// porch -
		{ BLACKMAG_X(2), BLACKMAG_Y(3) },	// porch 0
		{ BLACKMAG_X(1), BLACKMAG_Y(3) },	// porch +
		{ BLACKMAG_X(0), BLACKMAG_Y(3) },	// porch ++
		{ BLACKMAG_X(2), BLACKMAG_Y(4) },	// disconnected
	};
	//static const int NUM = GL_C_ARRAY_SIZE(UV);

	static const int PORCH = 7;
	static const int PAVE = 4;

	const CArray<Rectangle2I, SpaceTree::MAX_ZONES>& zones = spaceTree->Zones();
	for( int i=0; i<zones.Size(); ++i ) {
		Rectangle2I b = zones[i];

		for( int y=b.min.y; y<=b.max.y; ++y ) {
			for( int x=b.min.x; x<=b.max.x; ++x ) {

				// Check for memory exceeded and break.
				if ( voxelBuffer.Size()+4 >= voxelBuffer.Capacity() ) {
					//GLASSERT(0);	// not a problem, but may need to adjust capacity
					y = b.max.y+1;
					x = b.max.x+1;
					break;
				}

				const WorldGrid& wg = grid[INDEX(x,y)];
				int rotation = 0;

				if ( wg.Height() == 0 ) {
					int layer = wg.Land();
					if ( layer == WorldGrid::LAND ) {
						if (wg.Porch()) {
							layer = PORCH + wg.Porch() - 1;
						}
						else if (wg.Pave()) {
							layer = PAVE + wg.Pave() - 1;
						}
					}

					Vertex* vArr = voxelBuffer.PushArr( 4 );

					float fx = (float)x;
					float fy = (float)y;
					vArr[0].pos.Set( fx,		0, fy );
					vArr[1].pos.Set( fx,		0, fy+1.0f );
					vArr[2].pos.Set( fx+1.0f,	0, fy+1.0f );
					vArr[3].pos.Set( fx+1.0f,	0, fy );

					vArr[(0 + rotation)&3].tex.Set( UV[layer].x,	UV[layer].y );
					vArr[(1 + rotation)&3].tex.Set( UV[layer].x,	UV[layer].y+dv );
					vArr[(2 + rotation)&3].tex.Set( UV[layer].x+du,	UV[layer].y+dv );
					vArr[(3 + rotation)&3].tex.Set( UV[layer].x+du,	UV[layer].y );

					for( int i=0; i<4; ++i ) {
						vArr[i].normal = V3F_UP;
						vArr[i].boneID = 0;
					}
				}
			}
		}
	}
	gridVertexVBO->Upload( voxelBuffer.Mem(), voxelBuffer.Size()*sizeof(Vertex), 0 );
	nGrids = voxelBuffer.Size() / 4;
#undef BLACKMAG_X
}


Vertex* WorldMap::PushVoxelQuad( int id, const Vector3F& normal )
{
	Vertex* vArr = voxelBuffer.PushArr( 4 );

	// HARDCODE black magic values.
	static const float du = 0.125;
	#define BLACKMAG_X(x)	float( double(x*288 + 16) / 2048.0)

	float u = BLACKMAG_X(id);

	vArr[0].tex.Set(u, 0);
	vArr[1].tex.Set(u, 1);
	vArr[2].tex.Set(u + du, 1);
	vArr[3].tex.Set(u + du, 0);

	for( int i=0; i<4; ++i ) {
		vArr[i].normal = normal;
		vArr[i].boneID = 0;
	}
	return vArr;
#undef BLACKMAG_X
}

void WorldMap::PushVoxel( int id, float x, float z, float h, const float* walls )
{
	Vertex* v = PushVoxelQuad(id, V3F_UP);
	v[0].pos.Set( x, h, z );
	v[1].pos.Set( x, h, z+1.f );
	v[2].pos.Set( x+1.f, h, z+1.f );
	v[3].pos.Set( x+1.f, h, z );

	// duplicated in PrepVoxels
	static const float H = 0.5f;
	static const Vector2F delta[4] = { {H,0}, {0,H}, {-H,0}, {0,-H} };

	const Vector2F c = { x+0.5f, z+0.5f };

	for( int i=0; i<4; ++i ) {
		if ( walls[i] >= 0 ) {
			int j = (i+1)%4;

			const Vector2F v0 = c + delta[i] - delta[j];
			const Vector2F v1 = c + delta[i] + delta[j];
			float dH = h - walls[i];
			GLASSERT( dH > 0 );

			Vector3F normal = { delta[i].x*2.0f, 0.0f, delta[i].y*2.0f };
			Vertex* v = PushVoxelQuad( id, normal );
			v[0].pos.Set( v0.x, walls[i],	v0.y );
			v[1].pos.Set( v0.x, h,			v0.y );
			v[2].pos.Set( v1.x, h,			v1.y );
			v[3].pos.Set( v1.x, walls[i],	v1.y );

			v[1].tex.y = dH;
			v[2].tex.y = dH;
		}
	}
}


void WorldMap::PrepVoxels(const SpaceTree* spaceTree, grinliz::CDynArray<Model*>* models, const grinliz::Plane* planes6)
{
	//GRINLIZ_PERFTRACK
	//PROFILE_FUNC();
	// For each region of the spaceTree that is visible,
	// generate voxels.
	if ( !voxelVertexVBO ) {
		voxelVertexVBO = new GPUVertexBuffer( 0, sizeof(Vertex)*MAX_VOXEL_QUADS*4 );
	}
	voxelBuffer.Clear();
	nTrees = 0;
	ParticleSystem* ps = 0;
	const ParticleDef *fireDef = 0, *smokeDef = 0, *shockDef = 0;
	if (engine) {
		ps = engine->particleSystem;
		fireDef  = &ps->GetPD(ISC::fire);
		smokeDef = &ps->GetPD(ISC::smoke);
		shockDef = &ps->GetPD(ISC::shock);
	}

	const CArray<Rectangle2I, SpaceTree::MAX_ZONES>& zones = spaceTree->Zones();
	for( int i=0; i<zones.Size(); ++i ) {
		Rectangle2I b = zones[i];
		// Don't reach into the edge, which is used as an always-0 pad.
		if ( b.min.x == 0 ) b.min.x = 1;
		if ( b.max.x == width-1) b.max.x = width-2;
		if ( b.min.y == 0 ) b.min.y = 1;
		if ( b.max.y == height-1) b.max.y = height-2;

		for( int y=b.min.y; y<=b.max.y; ++y ) {
			for( int x=b.min.x; x<=b.max.x; ++x ) {

				// Check for memory exceeded and break.
				if ( voxelBuffer.Size()+(6*4) >= voxelBuffer.Capacity() ) {
					GLASSERT(0);	// not a problem, but may need to adjust capacity
					y = b.max.y+1;
					x = b.max.x+1;
					break;
				}

				// Generate rock, magma, or water.
				// Generate vericles down (but not up.)
				float wall[4] = { -1, -1, -1, -1 };
				float h = 0;
				int id = ROCK;
				const WorldGrid& wg = grid[INDEX(x,y)];

				if (wg.Plant()) {
					Rectangle3F aabb;
					aabb.Set(float(x), 0, float(y), float(x + 1), 2.5f, float(y + 1));
					float fraction = float(wg.HP()) / float(wg.TotalHP());
					int accept = true;
					for (int k = 0; k < 6; ++k) {
						if (ComparePlaneAABB(planes6[k], aabb) == NEGATIVE) {
							accept = false;
							break;
						}
					}
					if (accept) {
						Model* m = PushTree(x, y, wg.Plant() - 1, wg.PlantStage(), fraction);
						models->Push(m);

						Vector3F pos3f = { float(x)+0.5f, 0, float(y) + 0.5f };
						if (wg.PlantOnFire()) {
							if (ps) {
								ps->EmitPD(*smokeDef, pos3f, V3F_UP, STD_FRAME_TIME);
								ps->EmitPD(*fireDef, pos3f, V3F_UP, STD_FRAME_TIME);
							}
						}
						if (wg.PlantOnShock()) {
							if (ps) {
								ps->EmitPD(*shockDef, pos3f, V3F_UP, STD_FRAME_TIME);
							}
						}
					}
				}

				if (wg.IsFluid()) {
					id = (wg.FluidType() == WorldGrid::FLUID_WATER) ? POOL : MAGMA;;
					h = (float)wg.FluidHeight() - 0.125f;
					// Draw all walls:
					wall[0] = wall[1] = wall[2] = wall[3] = (float)wg.RockHeight();
					PushVoxel( id, (float)x, (float)y, h, wall ); 

					if (wg.RockHeight()) {
						h = (float)wg.RockHeight();
						wall[0] = wall[1] = wall[2] = wall[3] = 0;
						PushVoxel((wg.RockType() == WorldGrid::ROCK ? ROCK : ICE), (float)x, (float)y, h, wall);
					}
				}
				else if ( wg.Magma() ) {
					id = MAGMA;
					h = (float)wg.RockHeight();
					if ( h < 0.1f ) h = 0.1f;
					// Draw all walls:
					wall[0] = wall[1] = wall[2] = wall[3] = 0;
					PushVoxel( id, (float)x, (float)y, h, wall ); 

					Vector3F pos3f = { float(x)+0.5f, 0, float(y) + 0.5f };
					if (ps) {
						ps->EmitPD(*smokeDef, pos3f, V3F_UP, STD_FRAME_TIME);
					}
				}
				else if ( wg.RockHeight() ) {
					id = (wg.RockType() == WorldGrid::ROCK) ? ROCK : ICE;
					h = (float)wg.RockHeight();
					// duplicated in PushVoxel
					static const Vector2I delta[4] = { {1,0}, {0,1}, {-1,0}, {0,-1} };
					for( int k=0; k<4; ++k ) {
						const WorldGrid& next = grid[INDEX(x+delta[k].x, y+delta[k].y)];
						if (!next.IsFluid() && !next.Magma()) {
							// draw wall or nothing.
							if ( next.RockHeight() < wg.RockHeight() ) {
								wall[k] = (float)next.RockHeight();
							}
						}
						else {
							wall[k] = 0;
						}
					}
					PushVoxel( id, (float)x, (float)y, h, wall ); 
				}
			}
		}
	}
	for (int i = nTrees; i < treePool.Size(); ++i) {
		treePool[i]->SetFlag(Model::MODEL_INVISIBLE);
	}
	voxelVertexVBO->Upload( voxelBuffer.Mem(), voxelBuffer.Size()*sizeof(Vertex), 0 );
	nVoxels = voxelBuffer.Size() / 4;
}


void WorldMap::DrawVoxels( GPUState* state, const grinliz::Matrix4* xform )
{
	if ( !voxelTexture ) {
		voxelTexture = TextureManager::Instance()->GetTexture( "voxel" );
	}
	Vertex v;
	GPUStream stream( v );
	GPUDevice* device = GPUDevice::Instance();

	if ( xform ) {
		device->PushMatrix( GPUDevice::MODELVIEW_MATRIX );
		device->MultMatrix( GPUDevice::MODELVIEW_MATRIX, *xform );
	}

	// no fix, saturation, emmissive
	//Vector4F control = { 1, Saturation() * GRID_SATURATION, 1, 1 };
	GPUControlParam control;
	control.saturation = Saturation() * GRID_SATURATION;

	GPUStreamData data;
	data.vertexBuffer	= voxelVertexVBO->ID();
	data.texture0		= voxelTexture;
	data.controlParam	= &control;

	device->DrawQuads( *state, stream, data, nVoxels );

	if ( xform ) {
		device->PopMatrix( GPUDevice::MODELVIEW_MATRIX );
	}
}


void WorldMap::Draw3D(  const grinliz::Color3F& colorMult, StencilMode mode, bool useSaturation )
{
	GPUDevice* device = GPUDevice::Instance();

	// Real code to draw the map:
	FlatShader shader;
	shader.SetColor( colorMult.r(), colorMult.g(), colorMult.b() );
	shader.SetStencilMode( mode );
	if ( useSaturation ) {
		shader.SetShaderFlag( ShaderManager::SATURATION );
	}
	Submit( &shader );

	if ( debugRegionOverlay.Area() > 0 ) {
		if ( mode == STENCIL_CLEAR ) {
			// Debugging pathing zones:
			DrawZones();
		}
	}

#if 0
	if ( mode == STENCIL_CLEAR ) {
		DrawTreeZones();
	}
#endif

	if ( debugPathVector.Size() > 0 ) {
		FlatShader debug;
		debug.SetColor( 1, 0, 0, 1 );
		for( int i=0; i<debugPathVector.Size()-1; ++i ) {
			Vector3F tail = { debugPathVector[i].x, 0.2f, debugPathVector[i].y };
			Vector3F head = { debugPathVector[i+1].x, 0.2f, debugPathVector[i+1].y };
			device->DrawArrow( debug, tail, head, 0.2f );
		}
	}
	{
		// Debugging coordinate system:
		Vector3F origin = { 0, 0.1f, 0 };
		Vector3F xaxis = { 5, 0.1f, 0 };
		Vector3F zaxis = { 0, 0.1f, 5 };

		FlatShader debug;
		debug.SetColor( 1, 0, 0, 1 );
		device->DrawArrow( debug, origin, xaxis, 0.3f );
		debug.SetColor( 0, 0, 1, 1 );
		device->DrawArrow( debug, origin, zaxis, 0.3f );
	}
}


