#include "circuitsim.h"
#include "worldmap.h"
#include "lumosmath.h"
#include "lumoschitbag.h"
#include "lumosgame.h"
#include "mapspatialcomponent.h"
#include "gameitem.h"

#include "../engine/particle.h"
#include "../engine/engine.h"

#include "../xegame/spatialcomponent.h"
#include "../xegame/rendercomponent.h"
#include "../xegame/istringconst.h"
#include "../xegame/chitcontext.h"

#include "../Shiny/include/Shiny.h"

#include "../script/procedural.h"
#include "../script/batterycomponent.h"

#include "../gamui/gamui.h"
#include "../audio/xenoaudio.h"

using namespace grinliz;
using namespace gamui;

CircuitSim::CircuitSim(const ChitContext* _context, const Vector2I& _sector) : context(_context), sector(_sector)
{
	GLASSERT(context);
	dragStart.Zero();
	dragCurrent.Zero();
	enableOverlay = false;

	Random random;
	random.SetSeed(sector.x * 37 + sector.y * 41);
	ticker.SetPeriod(2000 + random.Rand(400));
	ticker.SetTime(ticker.Period());

	RenderAtom groupColor[NUM_GROUPS] = {
		LumosGame::CalcPaletteAtom(PAL_TANGERINE * 2, PAL_TANGERINE), // power
		LumosGame::CalcPaletteAtom(PAL_BLUE * 2, PAL_BLUE), // sensor
		LumosGame::CalcPaletteAtom(PAL_GREEN * 2, PAL_GREEN), // device
	};
	for (int i = 0; i < NUM_GROUPS; ++i) {
		RenderAtom atom = groupColor[i];

		canvas[0][i].Init(&context->worldMap->overlay0, atom);
		atom.renderState = (const void*)Map::RENDERSTATE_MAP_TRANSLUCENT;
		canvas[1][i].Init(&context->worldMap->overlay1, atom);

		canvas[0][i].SetLevel(-10 + i);
		canvas[1][i].SetLevel(-10 + i);
	}
}


CircuitSim::~CircuitSim()
{
}


void CircuitSim::Connection::Serialize(XStream* xs)
{
	XarcOpen(xs, "Connection");
	XARC_SER(xs, a);
	XARC_SER(xs, b);
	XarcClose(xs);
}


void CircuitSim::Particle::Serialize(XStream* xs)
{
	XarcOpen(xs, "Particle");
	XARC_SER_ENUM(xs, EParticleType, type);
	XARC_SER(xs, powerRequest);
	XARC_SER(xs, origin);
	XARC_SER(xs, pos);
	XARC_SER(xs, dest);
	XARC_SER(xs, delay);
	XarcClose(xs);
}


void CircuitSim::Serialize(XStream* xs)
{
	XarcOpen(xs, "CircuitSim");
	XARC_SER(xs, enableOverlay);
	XARC_SER_CARRAY(xs, connections);
	XARC_SER_CARRAY(xs, particles);

	// FIXME: serialize hashtable general solution?

	XarcClose(xs);
}


void CircuitSim::DragStart(const grinliz::Vector2F& v)
{
	dragStart = dragCurrent = v;
	ticker.SetReady();
}


void CircuitSim::Drag(const grinliz::Vector2F& v)
{
	dragCurrent = v;
	ticker.SetReady();
}


void CircuitSim::DragEnd(const grinliz::Vector2F& v)
{
	dragStart.Zero();
	dragCurrent.Zero();
	ticker.SetReady();
}


void CircuitSim::NewParticle(EParticleType type, int powerRequest, const grinliz::Vector2F& origin, const grinliz::Vector2F& dest, int delay)
{
	GLOUTPUT(("New particle at %f,%f\n", origin.x, origin.y));
	Particle p = { type, powerRequest, origin, origin, dest, delay };
	newQueue.Push(p);
}


Chit* CircuitSim::FindPower(const Vector2F& device)
{
	Vector2I sector = ToSector(device);
	CChitArray arr;
	context->chitBag->QueryBuilding(ISC::temple, SectorBounds(sector), &arr);

	Chit* best = 0;
	float bestScore = 0;

	for (int i = 0; i < arr.Size(); ++i) {
		Chit* temple = arr[i];
		Vector2F d = ToWorld2F(temple->Position()) - device;
		float len = d.Length();
		if (len == 0) continue;

		BatteryComponent* battery = (BatteryComponent*)temple->GetComponent("BatteryComponent");
		GLASSERT(battery);
		float charge = float(battery->Charge());
		float score = (charge + 1.0f) / len;
		if (score > bestScore) {
			bestScore = score;
			best = temple;
		}
	}
	return best;
}



void CircuitSim::FindConnections(const grinliz::Rectangle2I& group, grinliz::CDynArray<const Connection*> *out)
{
	out->Clear();
	for (const Connection& c : connections) {
		if (group.Contains(c.a) || group.Contains(c.b)) {
			out->Push(&c);
		}
	}
}


void CircuitSim::DeviceOn(Chit* building)
{
	if (!building) return;
	if (!building->GetRenderComponent()) return;
	if (!building->GetItem()) return;
	IString buildingName = building->GetItem()->IName();

	if (buildingName == ISC::turret) {
		Quaternion q = building->GetRenderComponent()->MainModel()->GetRotation();
		Matrix4 rot;
		q.ToMatrix(&rot);
		Vector3F straight = rot * V3F_OUT;
		RenderComponent* rc = building->GetRenderComponent();
		Vector3F trigger = { 0, 0, 0 };
		rc->CalcTrigger(&trigger, 0);

		XenoAudio::Instance()->PlayVariation(ISC::blasterWAV, building->ID(), &trigger);

		DamageDesc dd(15, GameItem::EFFECT_SHOCK);
		context->chitBag->NewBolt(trigger, straight, dd.effects, building->ID(),
								  dd.damage, 8.0f, false);
	}
	else if (buildingName == ISC::gate) {
		Vector2I pos = ToWorld2I(building->Position());
		context->worldMap->SetRock(pos.x, pos.y, 1, false, 0);
	}
	else if (buildingName == ISC::timedGate) {
		Vector2I pos = ToWorld2I(building->Position());
		context->worldMap->SetRock(pos.x, pos.y, 1, false, 0);
		GateTimer gateTimer(pos, 45 * 1000);
		gateTimers.Push(gateTimer);
	}
}


void CircuitSim::DeviceOff(Chit* building)
{
	if (!building) return;
	if (!building->GetRenderComponent()) return;
	if (!building->GetItem()) return;
	IString buildingName = building->GetItem()->IName();

	if (buildingName == ISC::gate) {
		Vector2I pos = ToWorld2I(building->Position());
		context->worldMap->SetRock(pos.x, pos.y, 0, false, 0);
	}
}

/*
	All connections are between a Device and something; so
	the type of connection is the not-device one. (Power or Sensor.)
	
	1. A sensor sends control particles to ALL attatched devices.
	2. When a device receives a control particle, sends a control particle with N requests to ONE attatched power.
	3. When a device receives a power particle, it activates. (round robin in group)
	4. When a power receives a control particle, it queues N power particles.
*/

void CircuitSim::ParticleArrived(const Particle& p)
{
	static const int POWER_DELAY = 250;
	int type = 0, index = 0;
	CChitArray arr;
	static const int NDEVICES = 3;
	IString deviceNames[NDEVICES] = { ISC::turret, ISC::gate, ISC::timedGate };

	if (FindGroup(ToWorld2I(p.dest), &type, &index)) {
		const grinliz::Rectangle2I& group = groups[type][index];
		if (type == DEVICE_GROUP && p.type == EParticleType::controlOn) {
			Chit* power = FindPower(ToWorld2F(group.Center()));
			if (power) {
				// request power
				ItemNameFilter deviceFilter(deviceNames, NDEVICES);

				context->chitBag->QuerySpatialHash(&arr, ToWorld2F(group), 0, &deviceFilter);
				NewParticle(EParticleType::controlOn, arr.Size(), p.pos, ToWorld2F(power->Position()));
			}
		}
		else if (type == DEVICE_GROUP && p.type == EParticleType::controlOff) {
			// Doesn't need power for off state.
			ItemNameFilter filter(ISC::gate);
			context->chitBag->QuerySpatialHash(&arr, ToWorld2F(group), 0, &filter);
			for (int i = 0; i < arr.Size(); ++i) {
				DeviceOff(arr[i]);
			}
		}
		else if (type == DEVICE_GROUP && p.type == EParticleType::power) {
			// activate device
			ItemNameFilter deviceFilter(deviceNames, NDEVICES);

			context->chitBag->QuerySpatialHash(&arr, ToWorld2F(group), 0, &deviceFilter);
			if (arr.Size()) {
				int value = 0;
				if (!roundRobbin.Query(group.min, &value)) {
					roundRobbin.Add(group.min, 0);
				}
				Chit* chit = arr[value % arr.Size()];
				++value;
				roundRobbin.Add(group.min, value);

				DeviceOn(chit);
			}
		}
		else if (type == POWER_GROUP && p.type == EParticleType::controlOn) {
			// Power group must have power device:
			Chit* building = context->chitBag->QueryBuilding(IString(), ToWorld2I(p.dest), 0);
			if (building) {
				BatteryComponent* battery = (BatteryComponent*)building->GetComponent("BatteryComponent");
				if (battery) {
					for (int i = 0; i < p.powerRequest && battery->UseCharge(); ++i) {
						NewParticle(EParticleType::power, 0, p.dest, p.origin, POWER_DELAY*i);
					}
				}
			}
		}
	}
}


void CircuitSim::TriggerSwitch(const grinliz::Vector2I& pos)
{
	Chit* building = context->chitBag->QueryPorch(pos);
	if (building && building->GetItem()) {
		IString buildingName = building->GetItem()->IName();
		Vector2I buildingPos = ToWorld2I(building->Position());

		if (buildingName == ISC::switchOn) {
			DoSensor(EParticleType::controlOn, buildingPos);
		}
		else if (buildingName == ISC::switchOff) {
			DoSensor(EParticleType::controlOff, buildingPos);
		}
	}
}


void CircuitSim::TriggerDetector(const grinliz::Vector2I& pos)
{
	Chit* building = context->chitBag->QueryBuilding(IString(), pos, 0);
	if (building) {
		const GameItem* item = building->GetItem();
		if (item) {
			if (item->IName() == ISC::detector) {
				DoSensor(EParticleType::controlOn, pos);
			}
		}
	}
}


void CircuitSim::DoSensor(EParticleType particle, const Vector2I& pos) 
{
	int type = 0, index = 0;
	if (FindGroup(pos, &type, &index) && type == SENSOR_GROUP) {
		const grinliz::Rectangle2I& group = groups[type][index];
		FindConnections(group, &queryConn);
		for (const Connection* c : queryConn) {
			Vector2I a = c->a;
			Vector2I b = c->b;
			if (!group.Contains(a)) {
				Swap(&a, &b);
			}

			Vector2F start = ToWorld2F(a);
			Vector2F end = ToWorld2F(b);
			NewParticle(particle, 0, start, end, 0);
		}
	}
}


void CircuitSim::DoTick(U32 delta)
{
	if (ticker.Delta(delta)) {
		CalcGroups();
		DrawGroups();
	}

	static const float SPEED = 6.0f;
	float travel = Travel(SPEED, delta);

	// Once again...don't mutate the array we are iterating on. *sigh*
	while (!newQueue.Empty()) {
		particles.Push(newQueue.Pop());
	}

	for (int i = 0; i < gateTimers.Size(); ++i) {
		gateTimers[i].time -= delta;
		if (gateTimers[i].time <= 0) {
			Vector2I pos = gateTimers[i].pos;
			Rectangle2I bounds(pos);
			Chit* gate = context->chitBag->QueryBuilding(ISC::timedGate, bounds, 0);
			if (gate) {
				context->worldMap->SetRock(pos.x, pos.y, 0, false, WorldGrid::SILICA);
			}
			gateTimers.SwapRemove(i);
			--i;
		}
	}

	for (int i = 0; i < particles.Size(); ++i) {
		Particle& p = particles[i];
		if (p.delay > 0) {
			p.delay -= int(delta);
			if (p.delay < 0) p.delay = 0;
			continue;
		}

		float len = (p.pos - p.dest).Length();
		if (len < travel) {
			ParticleArrived(p);
			particles.SwapRemove(i);
			--i;
		}
		else {
			static const int NPART = 2;
			static const float fraction = 1.0f / float(NPART);
			Vector2F normal = p.dest - p.pos;
			normal.Normalize();

			for (int j = 0; j < NPART; ++j) {
				Vector2F pos = p.pos + normal * (travel * float(j + 1)*fraction);
				IString particle = (p.type == EParticleType::power) ? ISC::power : ISC::control;
				context->engine->particleSystem->EmitPD(particle, ToWorld3F(pos), V3F_UP, delta);
			}
			p.pos += normal * travel;
		}
	}
}


void CircuitSim::FillGroup(grinliz::Rectangle2I* g, const Vector2I& pos, Chit* chit)
{
	MapSpatialComponent* msc = GET_SUB_COMPONENT(chit, SpatialComponent, MapSpatialComponent);
	GLASSERT(msc);

	g->DoUnion(msc->Bounds());
	hashTable.Remove(pos);
	for (int i = 0; i < 4; ++i) {
		Vector2I nextPos = pos.Adjacent(i);
		Chit* nextChit = 0;
		if (hashTable.Query(nextPos, &nextChit)) {
			FillGroup(g, nextPos, nextChit);
		}
	}
}


void CircuitSim::CleanConnections()
{
	// Rules:
	//	- only one connection between any group and another.
	//  - only one power correction between a device and a power source (not enforced, but particles only go to one temple.)

	for (int i = 0; i < connections.Size(); ++i) {
		Connection& c = connections[i];
		int type = 0;
		grinliz::Rectangle2I* groupA = 0;
		grinliz::Rectangle2I* groupB = 0;
		if (ConnectionValid(c.a, c.b, &type, &groupA, &groupB)) {
			c.sortA = groupA;
			c.sortB = groupB;
			if (c.sortB < c.sortA) {
				Swap(&c.a, &c.b);
				Swap(&c.sortA, &c.sortB);
			}
		}
		else {
			connections.SwapRemove(i);
			--i;
		}
	}
	connections.Sort([](const Connection& a, const Connection& b) {
		return a.sortA < b.sortA;
	});
	for (int i = 1; i < connections.Size(); ++i) {
		if (connections[i - 1].sortA == connections[i].sortA && connections[i - 1].sortB == connections[i].sortB) {
			connections.SwapRemove(i);
			--i;
		}
	}
}


void CircuitSim::CalcGroups()
{
	IString powerNames[]   = { ISC::temple, IString() };
	bool    powerGroups[]  = { false, false };
	IString sensorNames[]  = { ISC::detector, ISC::switchOn, ISC::switchOff, IString() };
	bool    sensorGroups[] = { true, false, false, false };
	IString deviceNames[]  = { ISC::turret, ISC::gate, ISC::timedGate, IString() };
	bool    deviceGroups[] = { true, true };

	IString* names[NUM_GROUPS] = { powerNames, sensorNames, deviceNames };
	const bool* doesGroup[NUM_GROUPS] = { powerGroups, sensorGroups, deviceGroups };

	hashTable.Clear();

	for (int i = 0; i < NUM_GROUPS; ++i) {
		groups[i].Clear();
		combinedArr.Clear();

		for (int j = 0; !names[i][j].empty(); ++j) {
			context->chitBag->FindBuilding(names[i][j], sector, 0, LumosChitBag::EFindMode::NEAREST, &queryArr, 0);
			if (doesGroup[i][j]) {
				for (Chit* chit : queryArr) {
					Vector2I pos = ToWorld2I(chit->Position());
					hashTable.Add(pos, chit);
					combinedArr.Push(chit);
				}
			}
			else {
				for (Chit* chit : queryArr) {
					MapSpatialComponent* msc = GET_SUB_COMPONENT(chit, SpatialComponent, MapSpatialComponent);
					GLASSERT(msc);
					grinliz::Rectangle2I* g = groups[i].PushArr(1);
					*g = msc->Bounds();
				}
			}
		}

		for (Chit* chit : combinedArr) {
			// If it is still in the hash table, then
			// it is a group.
			Vector2I pos = ToWorld2I(chit->Position());
			if (hashTable.Query(pos, 0)) {
				grinliz::Rectangle2I* g = groups[i].PushArr(1);
				MapSpatialComponent* msc = GET_SUB_COMPONENT(chit, SpatialComponent, MapSpatialComponent);
				GLASSERT(msc);
				*g = msc->Bounds();
				FillGroup(g, pos, chit);
			}
		}
	}
	CleanConnections();
}

void CircuitSim::DrawGroups()
{
	static float thickness = 1.0f / 32.0f;
	static float thicker = thickness * 2.0f;
	static float square = thickness * 4.0f;
	static float hSquare = square * 0.5f;
	static float gutter = thickness;
	static float half = thickness * 0.5f;
	static float arc = 1.0f / 16.0f;

	for (int i = 0; i < NUM_GROUPS; ++i) {
		canvas[0][i].Clear();
		for (const grinliz::Rectangle2I& group : groups[i]) {

			float x0 = float(group.min.x) + half + gutter;
			float y0 = float(group.min.y) + half + gutter;
			float x1 = float(group.max.x + 1) - half - gutter;
			float y1 = float(group.max.y + 1) - half - gutter;

			canvas[0][i].DrawRectangleOutline(x0, y0, x1 - x0, y1 - y0, thickness, arc);
		}
	}

	for (const Connection& c : connections) {
		// Groups come and go - need to find the group for the connection.
		grinliz::Rectangle2I* groupA = 0;
		grinliz::Rectangle2I* groupB = 0;
		int type = 0;
		if (ConnectionValid(c.a, c.b, &type, &groupA, &groupB)) {
			Vector2F p0 = ToWorld2F(c.a);
			Vector2F p1 = ToWorld2F(c.b);

			canvas[0][SENSOR_GROUP].DrawLine(p0.x, p0.y, p1.x, p1.y, thicker);
			canvas[0][SENSOR_GROUP].DrawRectangle(p0.x - hSquare, p0.y - hSquare, square, square);
			canvas[0][SENSOR_GROUP].DrawRectangle(p1.x - hSquare, p1.y - hSquare, square, square);
		}
	}

	if (!dragStart.IsZero()) {
		int type = 0;
		if (FindGroup(ToWorld2I(dragStart), &type, 0) && (type != POWER_GROUP)) {
			Vector2F p0 = dragStart;
			Vector2F p1 = dragCurrent;
			canvas[0][SENSOR_GROUP].DrawLine(p0.x, p0.y, p1.x, p1.y, thicker);
			canvas[0][SENSOR_GROUP].DrawRectangle(p0.x - hSquare, p0.y - hSquare, square, square);
			canvas[0][SENSOR_GROUP].DrawRectangle(p1.x - hSquare, p1.y - hSquare, square, square);
		}
	}

	for (int i = 0; i < NUM_GROUPS; ++i) {
		canvas[0][i].CopyCmdsTo(&canvas[1][i]);
		canvas[1][i].SetVisible(enableOverlay);
	}
}


bool CircuitSim::ConnectionValid(const Vector2I& a, const Vector2I& b, int *type, grinliz::Rectangle2I** groupA, grinliz::Rectangle2I** groupB)
{
	if (a == b) return false;	// can't connect to yourself.

	int indexA = -1, typeA = NUM_GROUPS;
	int indexB = -1, typeB = NUM_GROUPS;
	if (FindGroup(a, &typeA, &indexA) && FindGroup(b, &typeB, &indexB)) {
		if (   (typeA == DEVICE_GROUP && typeB == SENSOR_GROUP)
			|| (typeA == SENSOR_GROUP && typeB == DEVICE_GROUP)) 
		{
			if (type) *type = (typeA == DEVICE_GROUP) ? typeB : typeA;
			if (groupA) *groupA = &groups[typeA][indexA];
			if (groupB) *groupB = &groups[typeB][indexB];
			return true;
		}
	}
	return false;
}


void CircuitSim::Connect(const grinliz::Vector2I& a, const grinliz::Vector2I& b)
{
	int type = 0;
	if (ConnectionValid(a, b, &type, 0, 0)) {
		// If power, filter out power connection.
		Connection c = { a, b };
		connections.Push(c);
	}
	else {
		bool foundA = FindGroup(a, 0, 0);
		bool foundB = FindGroup(b, 0, 0);
		if (foundA && !foundB) {
			connections.Filter(a, [](const Vector2I& v, const Connection& c) {
				return c.a != v && c.b != v;
			});
		}
		else if (!foundA && foundB) {
			connections.Filter(b, [](const Vector2I& v, const Connection& c) {
				return c.a != v && c.b != v;
			});
		}
	}
	ticker.SetReady();
}


bool CircuitSim::FindGroup(const grinliz::Vector2I& pos, int* groupType, int* index)
{
	for (int j = 0; j < NUM_GROUPS; ++j) {
		for (int i = 0; i < groups[j].Size(); ++i) {
			if (groups[j][i].Contains(pos)) {
				if (groupType) *groupType = j;
				if (index) *index = i;
				return true;
			}
		}
	}
	return false;
}

