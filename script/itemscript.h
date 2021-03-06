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

#ifndef ITEM_SCRIPT_INCLUDED
#define ITEM_SCRIPT_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glcontainer.h"

#include "../game/gameitem.h"

class NewsHistory;

class ItemDefDB
{
public:
	ItemDefDB()		{ GLASSERT(!instance); instance = this; }
	~ItemDefDB()	{ GLASSERT(instance == this); instance = 0; }
	static ItemDefDB* Instance() { GLASSERT(instance); return instance; }

	void Load( const char* path );

	bool Has( const char* name );
	const GameItem& Get( const char* name, int intrinsic=-1 );

	typedef grinliz::CArray<const GameItem*, 16> GameItemArr;
	// Query an item and all its intrinsics; main item is
	// index 0, intrinsic items follow.
	void Get( const char* name, GameItemArr* arr );

	// Get the 'value' of the property 'prop' from the item with 'name'
	static void GetProperty( const char* name, const char* prop, int* value );

	void DumpWeaponStats();
	void AssignWeaponStats( const int* roll, const GameItem& base, GameItem* item );

	const grinliz::CDynArray< grinliz::IString >& GreaterMOBs() const { return greaterMOBs; }
	const grinliz::CDynArray< grinliz::IString >& LesserMOBs() const  { return lesserMOBs; }

private:
	static ItemDefDB* instance;

	grinliz::HashTable< const char*, GameItem*, grinliz::CompCharPtr, grinliz::OwnedPtrSem > map;
	// Names of all the items in the DefDB - "top" because "cyclops" is in the list, but not "cyclops claw"
	grinliz::CDynArray< grinliz::IString > topNames;
	grinliz::CDynArray< grinliz::IString > greaterMOBs, lesserMOBs;
};

// Needs to be small - lots of these to save.
struct ItemHistory
{
	ItemHistory() : itemID(0), level(0), value(0), kills(0), greater(0), crafted(0), score(0) {}

	bool operator<(const ItemHistory& rhs) const { return this->itemID < rhs.itemID; }
	bool operator==(const ItemHistory& rhs) const { return this->itemID == rhs.itemID; }

	int					itemID;
	grinliz::IString	titledName;
	U16					level;
	U16					value;
	U16					kills;
	U16					greater;
	U16					crafted;
	U16					score;

	void Set( const GameItem* );
	void Serialize( XStream* xs );
	void AppendDesc( grinliz::GLString* str, NewsHistory* history, const char* separator ) const;
};

/*
	As of this point, GameItems are owned by Chits. (Specifically ItemComponents). And
	occasionally other places. I'm questioning this design. However, we need to be
	able to look up items for history and such. It either is in use, or deleted, and
	we need a record of what it was.
*/
class ItemDB
{
public:
	ItemDB()	{ GLASSERT(instance == 0); instance = this; }
	~ItemDB()	{ GLASSERT(instance == this); instance = 0; }
	static ItemDB* Instance() { return instance;  }

	void Add( const GameItem* );		// start tracking (possibly insignificant)
	void Update( const GameItem* );		// item changed
	void Remove( const GameItem* );		// stop tracking

	// Which IC - if any - is holding the item.
	//void TrackItemComponent(const GameItem* item, const ItemComponent* ic);

	// Find an item by id; if null, can use History
	const GameItem*	Active( int id );
	//const ItemComponent* ActiveItemComponent(const GameItem* item);

	bool Registry(int id, ItemHistory* h);

	void Serialize( XStream* xs );

	int NumRegistry() const { return itemRegistry.Size(); }
	const ItemHistory& RegistryByIndex(int i) { return itemRegistry.GetValue(i); }

private:
	static ItemDB* instance;
	grinliz::HashTable< int, const GameItem* > itemMap;							// map of all the active, allocated items.
	grinliz::HashTable<int, ItemHistory> itemRegistry; // all the significant items ever created
};




#endif // ITEM_SCRIPT_INCLUDED