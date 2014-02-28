#include "farmscript.h"

#include "../xegame/chit.h"
#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/istringconst.h"

#include "../grinliz/glrectangle.h"

#include "../game/mapspatialcomponent.h"
#include "../game/lumoschitbag.h"
#include "../game/gameitem.h"

#include "../script/plantscript.h"
#include "../script/itemscript.h"

using namespace grinliz;

static const int	FARM_SCRIPT_CHECK = 2000;
// FIXME: not tuned
static const int	FRUIT_SELF_DESTRUCT = 60*1000;

FarmScript::FarmScript() : timer( 2000 )
{
	fruitGrowth = 0;
	farmBounds.Zero();
	efficiency = 0;
}


void FarmScript::Serialize( XStream* xs )
{
	XarcOpen( xs, ScriptName() );
	XARC_SER( xs, fruitGrowth );
	timer.Serialize( xs, "timer" );
	XarcClose( xs );
}


void FarmScript::Init()
{
	timer.SetPeriod( FARM_SCRIPT_CHECK + scriptContext->chit->random.Rand(FARM_SCRIPT_CHECK/16));
}


int FarmScript::GrowFruitTime( int stage, int nPlants )
{
	int stage2   = (stage+1)*(stage+1);
	int time = GROWTH_NEEDED / (stage2 * nPlants);
	return time;
}


void FarmScript::ComputeFarmBound()
{
	MapSpatialComponent* msc = GET_SUB_COMPONENT(scriptContext->chit, SpatialComponent, MapSpatialComponent);
	Vector2I pos2i = msc->MapPosition();
	farmBounds.min = farmBounds.max = pos2i;
	farmBounds.Outset(FARM_GROW_RAD);

	// Other farms clip this if they are too close.
	// 01234567
	// FffgG		If within 4.eps, may impact us.

	float rad = float(FARM_GROW_RAD)*2.f + 0.1f;
	ItemNameFilter filter(IStringConst::farm);
	CChitArray array;
	scriptContext->chitBag->QuerySpatialHash(&array, ToWorld2F(pos2i), rad, 0, &filter);

	for (int i = 0; i < array.Size(); ++i) {
		// We are only affected by Chits with IDs greater than our own, so
		// that clipping doesn't affect both chits.
		Chit* chit = array[i];
		if (chit->ID() > scriptContext->chit->ID()) {
			Vector2I q2i = chit->GetSpatialComponent()->GetPosition2DI();
			Rectangle2I qBounds;
			qBounds.min = qBounds.max = q2i;
			qBounds.Outset(FARM_GROW_RAD);

			if (farmBounds.Intersect(qBounds)) {
				if (q2i.x > pos2i.x)
					farmBounds.max.x = Min(farmBounds.max.x, qBounds.min.x);
				if (q2i.x < pos2i.x)
					farmBounds.min.x = Max(farmBounds.min.x, qBounds.max.x);
				if (q2i.y > pos2i.y)
					farmBounds.max.y = Min(farmBounds.max.y, qBounds.min.y);
				if (q2i.y < pos2i.y)
					farmBounds.min.y = Max(farmBounds.min.y, qBounds.max.y);
			}
		}
	}
}

int FarmScript::DoTick( U32 delta )
{
	int n = timer.Delta( delta );
	MapSpatialComponent* msc = GET_SUB_COMPONENT( scriptContext->chit, SpatialComponent, MapSpatialComponent );

	while ( n && msc ) {
		--n;

		ComputeFarmBound();

		Rectangle2F bounds;
		bounds.Set(float(farmBounds.min.x) - 0.1f, float(farmBounds.min.y) - 0.1f, float(farmBounds.max.x) + 0.1f, float(farmBounds.max.y) + 0.1f);
		
		PlantFilter filter;
		CChitArray plantArr;
		GLASSERT(plantArr.Capacity() >= FARM_GROW_RAD*FARM_GROW_RAD - 1);
		scriptContext->chitBag->QuerySpatialHash( &plantArr, bounds, 0, &filter );
		
		int growth = 0;

		for( int i=0; i<plantArr.Size(); ++i ) {
			Chit* chit = plantArr[i];
			int type = 0, stage = 0;
			GameItem* plantItem = PlantScript::IsPlant( chit, &type, &stage );
			GLASSERT( plantItem );

			int g = (stage + 1)*(stage + 1);
			if (   (plantItem->flags & GameItem::FLAMMABLE) 
				|| (plantItem->flags & GameItem::SHOCKABLE)) 
			{
				g *= 2;	// bonus for volitility
			}
			growth += g;
		}

		fruitGrowth += growth * timer.Period();
		const int AREA = (FARM_GROW_RAD * 2 + 1)*(FARM_GROW_RAD * 2 + 1) - 1;
		efficiency = 100 * growth / (PlantScript::NUM_STAGE * PlantScript::NUM_STAGE * AREA);
	}

	while ( fruitGrowth >= GROWTH_NEEDED ) {
		fruitGrowth -= GROWTH_NEEDED;

		Rectangle2I r = msc->PorchPos();

		const GameItem& def = ItemDefDB::Instance()->Get( "fruit" );
		GameItem* gameItem = new GameItem( def );
		scriptContext->chitBag->NewItemChit( ToWorld3F( r.min ),
											 gameItem,
											 true,
											 true,
											 FRUIT_SELF_DESTRUCT );
	}

	RenderComponent* rc = scriptContext->chit->GetRenderComponent();
	if (rc) {
		CStr<32> str;
		//str.Format("%d,%d-%d,%d e=%d", farmBounds.min.x, farmBounds.min.y, farmBounds.max.x, farmBounds.max.y, efficiency);
		str.Format("A=%d %d%% [%d%%]", farmBounds.Area(), efficiency, fruitGrowth*100/GROWTH_NEEDED);
		rc->SetDecoText(str.c_str());
	}

	return timer.Next();
}
