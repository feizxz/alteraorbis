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

#ifndef LUMOS_VISITOR_INCLUDED
#define LUMOS_VISITOR_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/glrandom.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glcontainer.h"
#include "sectorport.h"

class XStream;
class WorldMap;

// Each visitor has their own personality and memory, seperate
// from the chit AI.
struct VisitorData
{
	VisitorData() : id( 0 ) {}
	void Serialize( XStream* xs );

	void Connect() {
		sectorVisited.Clear();
		kioskTime = 0;
	}

	enum {	NUM_VISITS = 4, 			// how many domains would like to be visited before disconnect
			KIOSK_TIME = 5000,
			MEMORY = 4
	};
	int id;								// chit id, and whether in-world or not.
	U32 kioskTime;						// time spent standing at current kiosk
	grinliz::CArray< grinliz::Vector2I, NUM_VISITS > sectorVisited;	// which sectors we have visited a kiosk at. 

	struct Memory {
		Memory() { sector.Zero(); rating=0; }

		grinliz::Vector2I	sector;
		int					rating;		
		void Serialize( XStream* xs );
	};
	grinliz::CArray< Memory, MEMORY > memoryArr;

	void NoKiosk( const grinliz::Vector2I& sector );
	void DidVisitKiosk( const grinliz::Vector2I& sector );
};


class Visitors
{
public:
	Visitors();
	~Visitors();

	void Serialize( XStream* xs );

	enum { NUM_VISITORS = 200 };
	VisitorData visitorData[NUM_VISITORS];

	static VisitorData* Get( int index );
	// only returns existing; doesn't create.
	static Visitors* Instance()	{ return instance; }

	SectorPort ChooseDestination( int visitorIndex, WorldMap* map );

private:
	static Visitors* instance;
	grinliz::Random random;
};

#endif // LUMOS_VISITOR_INCLUDED