#include "workqueue.h"
#include "worldmap.h"
#include "lumosgame.h"
#include "lumoschitbag.h"
#include "gameitem.h"
#include "worldinfo.h"

#include "../engine/engine.h"

#include "../xegame/istringconst.h"
#include "../xegame/chitcontext.h"

#include "../xarchive/glstreamer.h"

#include "../script/itemscript.h"
#include "../script/buildscript.h"
#include "../script/corescript.h"

#include "../ai/tasklist.h"
#include "../game/aicomponent.h"

using namespace grinliz;
using namespace gamui;

static const float NOTIFICATION_RAD = 5.0f;

WorkQueue::WorkQueue()
{
	parentChit = 0;
	sector.Zero();
}


WorkQueue::~WorkQueue()
{
	GLASSERT( parentChit );
	for (int i = 0; i < queue.Size(); ++i) {
		delete[] queue[i].image;
	}
}


int WorkQueue::CalcTaskSize( const IString& structure )
{
	int size = 1;
	if ( !structure.empty() ) {
		if ( structure == ISC::pave || structure == ISC::ice ) {
			size = 1;
		}
		else {
			const GameItem& gameItem = ItemDefDB::Instance()->Get( structure.c_str() );
			gameItem.keyValues.Get( ISC::size, &size );
		}
	}
	return size;
}


void WorkQueue::AddImage( QueueItem* item )
{
	BuildScript buildScript;
	const BuildData& buildData = buildScript.GetData(item->buildScriptID);
	int size = buildData.size;

	GLASSERT(!item->image);
	item->image = new gamui::Image[3];
	for (int i = 0; i < 3; ++i) {
		item->image[i].Init(&parentChit->Context()->worldMap->overlay1, RenderAtom(), false);
	}

	GLASSERT( parentChit );

	RenderAtom atom;
	RenderAtom porchAtom;
	if (item->buildScriptID == BuildScript::CLEAR) {
		atom = LumosGame::CalcIconAtom("delete");
		item->image[0].SetAtom(atom);
		item->image[0].SetPos((float)item->pos.x, (float)item->pos.y);
		item->image[0].SetSize((float)size, (float)size);
		item->image[0].SetVisible(true);
	}
	else {
		BuildData::DrawBounds(item->Bounds(), item->PorchBounds(), item->image);
	}
}


void WorkQueue::RemoveImage( QueueItem* item )
{
	delete [] item->image;
	item->image = 0;
}


void WorkQueue::Remove(const grinliz::Vector2I& pos)
{
	for (int i = 0; i < queue.Size(); ++i) {
		Rectangle2I bounds = queue[i].Bounds();

		if (bounds.Contains(pos)) {
			RemoveItem(i);
		}
	}
}


bool WorkQueue::AddAction(const grinliz::Vector2I& pos2i, int buildScriptID, float rotation, int variation)
{
	if ( ToSector( pos2i ) != sector ) {
		// wrong sector.
		return false;
	}

	QueueItem item;
	item.buildScriptID = buildScriptID;
	item.pos = pos2i;
	item.rotation = rotation;
	item.variation = variation;

#ifdef DEBUG
	if (item.buildScriptID == BuildScript::PAVE) GLASSERT(item.variation);
#endif

	if ( !TaskCanComplete( item )) {
		return false;
	}

	// Clear out existing.
	Remove( pos2i );
	AddImage( &item );
	queue.Push( item );
	SendNotification( pos2i );
	return true;
}


void WorkQueue::SendNotification( const grinliz::Vector2I& pos2i )
{
	GLASSERT( parentChit );
	LumosChitBag* chitBag = parentChit->Context()->chitBag;

	Vector2F pos2 = { (float)pos2i.x+0.5f, (float)pos2i.y+0.5f };
	// Notify near.
	CChitArray array;
	ItemNameFilter workerFilter( ISC::worker);
	chitBag->QuerySpatialHash( &array, pos2, NOTIFICATION_RAD, 0, &workerFilter );
	for( int i=0; i<array.Size(); ++i ) {
		ChitMsg msg( ChitMsg::WORKQUEUE_UPDATE );
		array[i]->SendMessage( msg );
	}
}

	
void WorkQueue::Assign( int id, const WorkQueue::QueueItem* item )
{
	int index = item - queue.Mem();
	GLASSERT( index >= 0 && index < queue.Size() );
	GLASSERT( queue[index].assigned == 0 );
	queue[index ].assigned = id;
}


const WorkQueue::QueueItem* WorkQueue::GetJob( int id )
{
	for( int i=0; i<queue.Size(); ++i ) {
		if ( queue[i].assigned == id ) {
			GLASSERT( queue[i].image );
			return &queue[i];
		}
	}
	return 0;
}


bool WorkQueue::HasAssignedJob() const
{
	for (int i = 0; i < queue.Size(); ++i) {
		if (queue[i].assigned)
			return true;
	}
	return false;
}

const WorkQueue::QueueItem* WorkQueue::HasJobAt(const Vector2I& v) {
	for (int i = 0; i < queue.Size(); ++i) {
		if (queue[i].Bounds().Contains(v)) {
			return &queue[i];
		}
	}
	return nullptr;
}


const WorkQueue::QueueItem* WorkQueue::HasPorchAt(const Vector2I& v) {
	for (int i = 0; i < queue.Size(); ++i) {
		if (queue[i].PorchBounds().Contains(v)){
			return &queue[i];
		}
	}
	return nullptr;
}


const WorkQueue::QueueItem* WorkQueue::Find( const grinliz::Vector2I& chitPos )
{
	GLASSERT( parentChit );
	const ChitContext* context = parentChit->Context();
	WorldMap* worldMap = context->worldMap;

	int best=-1;
	float bestCost = FLT_MAX;
	const Vector2F start = { (float)chitPos.x+0.5f, (float)chitPos.y+0.5f };

	for( int i=0; i<queue.Size(); ++i ) {
		if ( queue[i].assigned == 0 ) {
			float cost = 0;
			Vector2F bestEnd = { 0, 0 };

			bool okay = worldMap->CalcWorkPath(start, queue[i].Bounds(), &bestEnd, &cost);
			if (okay && (cost < bestCost)) {
				bestCost = cost;
				best = i;
			}
		}
	}
	if ( best >= 0 ) {
		return &queue[best];
	}
	return 0;
}


void WorkQueue::RemoveItem( int index )
{
	if (queue[index].assigned) {
		LumosChitBag* chitBag = parentChit->Context()->chitBag;
		Chit* chit = chitBag->GetChit(queue[index].assigned);
		if (chit) {
			chit->GetAIComponent()->ClearTaskList();
		}
	}

	GLASSERT( queue[index].image );
	RemoveImage( &queue[index] );
	queue.Remove( index );
}


bool WorkQueue::TaskCanComplete( const WorkQueue::QueueItem& item )
{
	GLASSERT( parentChit );
	LumosChitBag* chitBag = parentChit->Context()->chitBag;
	const ChitContext* context = parentChit->Context();
	WorldMap* worldMap = context->worldMap;


	Wallet wallet;
	Vector2I sector = ToSector( item.pos );
	CoreScript* coreScript = CoreScript::GetCore( sector );

	Chit* controller = coreScript->ParentChit();
	if ( controller && controller->GetItem() ) {
		wallet = controller->GetItem()->wallet;
	}

	return WorkQueue::TaskCanComplete(	worldMap, 
										chitBag, 
										item.pos, 
										item.buildScriptID,
										wallet );
}


/*static*/ bool WorkQueue::TaskCanComplete(WorldMap* worldMap,
										   LumosChitBag* chitBag,
										   const grinliz::Vector2I& pos2i,
										   int action,
										   const Wallet& available)
{
//	Vector2F pos2 = { (float)pos2i.x + 0.5f, (float)pos2i.y + 0.5f };
	const WorldGrid& wg = worldMap->GetWorldGrid(pos2i.x, pos2i.y);

	BuildScript buildScript;
	const BuildData& buildData = buildScript.GetData(action);
	int size = buildData.size;
	const SectorData& sectorData = worldMap->GetSectorData(ToSector(pos2i));

	if (available.Gold() < buildData.cost) {
		return false;
	}

	int removable = 0;
	int water = 0;
	int building = 0;

	for (int y = pos2i.y; y < pos2i.y + size; ++y) {
		for (int x = pos2i.x; x < pos2i.x + size; ++x) {
			if (!worldMap->IsLand(x, y)) {
				++water;
			}
			// HACK: treat cores as water.
			// Can't touch them.
			if (sectorData.core.x == x && sectorData.core.y == y) {
				++water;
			}


			Vector2I v = { x, y };
			const WorldGrid& wg = worldMap->GetWorldGrid(x, y);

			// Check for the forbidden areas:
			if (wg.IsWater() || wg.IsPort() || wg.IsGrid()) {
				return false;
			}

			// The 'build' actions will automatically clear plants. (As will PAVE, etc.)
			// However, if the action is CLEAR, we need to know there is something
			// there to remove.
			if (wg.Plant()) {
				++removable;	// plant
			}
			else if (wg.RockHeight()) {
				++removable;
			}
			else if (chitBag->QueryBuilding(IString(),v,0)) {
				++building;	// building
			}
		}
	}

	if (water > 0) {
		return false;
	}

	if (action == BuildScript::CLEAR) {
		if ((removable + building) == 0) {
			// nothing to clear. (unless paved or circuit)
			return wg.Pave() != 0;
		}
	}
	else {
		if (building) {
			// stuff in the way
			return false;
		}
	}
	return true;
}

bool WorkQueue::TaskIsComplete(const WorkQueue::QueueItem& item)
{
	const ChitContext* context = parentChit->Context();
	const WorldGrid& wg = context->worldMap->GetWorldGrid(item.pos.x, item.pos.y);
	BuildScript buildScript;
	const BuildData& buildData = buildScript.GetData(item.buildScriptID);

	if (wg.IsWater()) {
		GLASSERT(false);	// shouldn't be work here.
		return true;	
	}

	if (BuildScript::IsClear(item.buildScriptID)) {
		if (wg.Plant()) {
			return false;
		}
		if (context->chitBag->QueryBuilding(IString(), item.pos, 0)) {
			return false; // need to clear building
		}
		if (wg.RockHeight() || wg.Pave()) {
			return false; // need to clear rock or pave
		}
		return true;
	}
	else if (BuildScript::IsBuild(item.buildScriptID)) {
		if (item.buildScriptID == BuildScript::PAVE) {
			GLASSERT(item.variation);
			return (wg.Pave() == item.variation) || item.variation == 0;
		}
		else if (item.buildScriptID == BuildScript::ICE) {
			return wg.RockHeight() > 0;
		}
		else {
			Chit* building = context->chitBag->QueryBuilding(IString(),item.pos,0);
			if (building && building->GetItem() && building->GetItem()->IName() == buildData.structure) {
				return true;
			}
			return false;
		}
	}
	GLASSERT(0);
	return true;
}


void WorkQueue::DoTick()
{
	GLASSERT( parentChit );
	LumosChitBag* chitBag = parentChit->Context()->chitBag;

	for( int i=0; i<queue.Size(); ++i ) {

		if (TaskIsComplete(queue[i])) {
			RemoveItem(i);
			--i;
			continue;
		}

		if (!TaskCanComplete(queue[i])) {
			RemoveItem(i);
			--i;
			continue;
		}

		if (queue[i].assigned) {
			Chit* chit = chitBag->GetChit(queue[i].assigned);
			if (!chit) {
				queue[i].assigned = 0;
			}
			// Chit exists - but is it doing this WorkItem?
			if (chit && chit->GetAIComponent()) {
				const ai::Task* task = chit->GetAIComponent()->GoalTask();
				if (task 
					&& (task->action == ai::Task::TASK_BUILD) 
					&& (task->buildScriptID == queue[i].buildScriptID) 
					&& task->pos2i == queue[i].pos) 
				{
					// Looks good! This is the work queue item.
				}
				else {
					// Something shifted.
					queue[i].assigned = 0;
				}
			}
		}
	}
}


WorkQueue::QueueItem::QueueItem()
{
	buildScriptID = 0;
	pos.Zero();
	assigned = 0;
	rotation = 0;
	variation = 0;
	image = 0;
}


void WorkQueue::QueueItem::Serialize( XStream* xs )
{
	XarcOpen( xs, "QueueItem" );
	XARC_SER(xs, buildScriptID);
	XARC_SER( xs, pos );
	XARC_SER( xs, assigned );
	XARC_SER( xs, rotation );
	XARC_SER( xs, variation );
	XarcClose( xs );
}


Rectangle2I WorkQueue::QueueItem::Bounds() const
{
	BuildScript buildScript;
	const BuildData& buildData = buildScript.GetData(buildScriptID);
	return BuildData::Bounds(buildData.size, pos);
}


Rectangle2I WorkQueue::QueueItem::PorchBounds() const
{
	BuildScript buildScript;
	const BuildData& buildData = buildScript.GetData(buildScriptID);
	if (buildData.porch) {
		int rot0_3 = LRint(NormalizeAngleDegrees(rotation) / 90.0f);
		return BuildData::PorchBounds(buildData.size, pos, rot0_3);
	}
	return Rectangle2I();
}


void WorkQueue::Serialize( XStream* xs ) 
{
	XarcOpen( xs, "WorkQueue" );
	XARC_SER( xs, sector );
	XARC_SER_CARRAY( xs, queue );
	XarcClose( xs );
}


void WorkQueue::InitSector( Chit* _parent, const grinliz::Vector2I& _sector )
{ 
	parentChit = _parent; 
	sector = _sector; 
	for( int i=0; i<queue.Size(); ++i ) {
		AddImage( &queue[i] );
	}
}

