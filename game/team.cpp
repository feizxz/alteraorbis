#include "team.h"
#include "../grinliz/glutil.h"
#include "../xegame/istringconst.h"
#include "../xegame/chit.h"
#include "../xegame/itemcomponent.h"
#include "../xarchive/glstreamer.h"

using namespace grinliz;

int Team::idPool = 1;	// id=0 is rogue.

grinliz::IString Team::TeamName(int team)
{
	IString name;
	CStr<64> str;
	int group = 0, id = 0;
	SplitID(team, &group, &id);

	switch (group) {
		case TEAM_HOUSE:
		str.Format("House-%x", id);
		name = StringPool::Intern(str.c_str());
		break;

		case TEAM_TROLL:
		// Since Trolls can't build anything,
		// any troll core is by definition
		// Truulga. (At least at this point.)
		name = ISC::Truulga;
		break;

		case TEAM_GOB:
		str.Format("Gob-%x", id);
		name = StringPool::Intern(str.c_str());
		break;

		default:
		break;
	}

	return name;
}


int Team::GetTeam( const grinliz::IString& itemName )
{
	if (itemName == ISC::trilobyte) {
		return TEAM_RAT;
	}
	else if ( itemName == ISC::mantis ) {
		return TEAM_GREEN_MANTIS;
	}
	else if ( itemName == ISC::redMantis ) {
		return TEAM_RED_MANTIS;
	}
	else if ( itemName == ISC::troll ) {
		return TEAM_TROLL;
	}
	else if (itemName == ISC::gob) {
		return TEAM_GOB;
	}
	else if (    itemName == ISC::cyclops
		      || itemName == ISC::fireCyclops
		      || itemName == ISC::shockCyclops )
	{
		return TEAM_CHAOS;
	}
	GLASSERT(0);
	return TEAM_NEUTRAL;
}


int Team::GetRelationship( int _t0, int _t1 )
{
	int t0 = 0, t1 = 0;
	SplitID(_t0, &t0, 0);
	SplitID(_t1, &t1, 0);

	// t0 <= t1 to keep the logic simple.
	if ( t0 > t1 ) Swap( &t0, &t1 );

	// CHAOS hates all - even each other.
	if ( t0 == TEAM_CHAOS || t1 == TEAM_CHAOS)
		return RELATE_ENEMY;
	// Likewise, neutral is neutral even to themselves.
	if (t0 == TEAM_NEUTRAL || t1 == TEAM_NEUTRAL)
		return RELATE_NEUTRAL;

	static const int F = RELATE_FRIEND;
	static const int E = RELATE_ENEMY;
	static const int N = RELATE_NEUTRAL;
	static const int OFFSET = TEAM_RAT;
	static const int NUM = NUM_TEAMS - OFFSET;

	static const int relate[NUM][NUM] = {
		{ F, E, E, E, E, E, E },		// rat
		{ 0, F, E, N, E, E, E },		// green
		{ 0, 0, F, N, E, E, E },		// red
		{ 0, 0, 0, F, E, N, E },		// troll 
		{ 0, 0, 0, 0, F, N, F },		// house
		{ 0, 0, 0, 0, 0, F, N },		// gobmen
		{ 0, 0, 0, 0, 0, 0, F },		// visitor
	};
	GLASSERT(t0 - OFFSET >= 0 && t0 - OFFSET < NUM);
	GLASSERT(t1 - OFFSET >= 0 && t1 - OFFSET < NUM);
	GLASSERT(t1 >= t0);
	return relate[t0-OFFSET][t1-OFFSET];
}


int Team::GetRelationship( Chit* chit0, Chit* chit1 )
{
	if ( chit0->GetItem() && chit1->GetItem() ) {
		return GetRelationship( chit0->GetItem()->team,
								chit1->GetItem()->team );
	}
	return RELATE_NEUTRAL;
}


void Team::Serialize(XStream* xs)
{
	XarcOpen(xs,"Team");
	XARC_SER(xs, idPool);
	XarcClose(xs);
}
