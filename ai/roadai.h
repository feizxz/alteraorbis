#ifndef ROAD_AI_INCLUDED
#define ROAD_AI_INCLUDED

#include "../grinliz/glvector.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glcontainer.h"
#include "../grinliz/glbitarray.h"
#include "../game/gamelimits.h"

class MapSpatialComponent;

class RoadAI
{
public:
	RoadAI(int originX, int originY);
	~RoadAI();

	void AddRoad(const grinliz::Vector2I* v, int n);
	void AddPlaza(const grinliz::Rectangle2I& rect);

	enum {MAX_ROADS = 8};
	const grinliz::Vector2I* GetRoad(int i, int *len);
	enum {MAX_PLAZA = 4};
	const grinliz::Rectangle2I* GetPlaza(int i);;

	void Begin(int n=1);
	bool Done();
	void Next();

	struct BuildZone {
		BuildZone() : roadID(0), roadDistance(0), rotation(0) { fullBounds.Set(0, 0, 0, 0); buildBounds.Set(0, 0, 0, 0); porchBounds.Set(0, 0, 0, 0); }

		int roadID;
		int roadDistance;
		int rotation;
		grinliz::Rectangle2I fullBounds;
		grinliz::Rectangle2I buildBounds;
		grinliz::Rectangle2I porchBounds;
	};
	const BuildZone* CalcZones(int* n, int size, bool porch);

	// fullBounds: 5x5 bounds
	// buildBounds: the 1x1 solar farm
	// porchbounds: where the access road should go
	const BuildZone* CalcFarmZones(int* n);

	bool IsOnRoad(MapSpatialComponent* msc, bool farm);
	bool IsOnRoad(const grinliz::Rectangle2I& bounds, bool porch, int rotation, bool farm);
	
private:
	bool UseMapEmpty(const grinliz::Rectangle2I& w);

	grinliz::CDynArray<grinliz::Vector2I> road[MAX_ROADS];
	grinliz::CArray<BuildZone, MAX_ROADS * 2> buildZones;
	grinliz::BitArray<SECTOR_SIZE, SECTOR_SIZE, 1> useMap;
	grinliz::CArray<grinliz::Rectangle2I, MAX_PLAZA> plazas;

	int itN;
	grinliz::Vector2I offset;
};


#endif //  ROAD_AI_INCLUDED
