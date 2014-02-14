#include "aineeds.h"
#include "../grinliz/glutil.h"
#include "../xarchive/glstreamer.h"

using namespace ai;
using namespace grinliz;

static const double DECAY_TIME = 200.0;	// in seconds

/* static */ const char* Needs::Name( int i )
{
	GLASSERT( i >= 0 && i < NUM_NEEDS );

	static const char* name[NUM_NEEDS] = { "food", "social", "energy", "fun" };
	GLASSERT( GL_C_ARRAY_SIZE( name ) == NUM_NEEDS );
	return name[i];
}


Needs::Needs()  
{ 
	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] = 1; 
	}
	morale = 1;
}



void Needs::DoTick( U32 delta, bool inBattle )
{
	double dNeed = double(delta) * 0.001 / DECAY_TIME;

	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] -= dNeed;
	}

	if ( inBattle ) {
		need[FUN] += dNeed * 10.0;
	}
	ClampNeeds();

	double min = need[0];
	double max = need[0];
	for( int i=1; i<NUM_NEEDS; ++i ) {
		min = Min( need[i], min );
		max = Max( need[i], max );
	}

	if ( min < 0.001 ) {
		morale -= dNeed;
	}
	if ( min > 0.5 ) {
		morale += dNeed;
	}

	morale = Clamp( morale, 0.0, 1.0 );
}


void Needs::ClampNeeds()
{
	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] = Clamp( need[i], 0.0, 1.0 );
	}
}


void Needs::Add( const Needs& other, double scale )
{
	for( int i=0; i<NUM_NEEDS; ++i ) {
		need[i] += other.need[i] * scale;
	}
	ClampNeeds();
}


void Needs::Serialize( XStream* xs )
{
	XarcOpen( xs, "Needs" );
	XARC_SER( xs, morale );
	XARC_SER_ARR( xs, need, NUM_NEEDS );
	XarcClose( xs );
}