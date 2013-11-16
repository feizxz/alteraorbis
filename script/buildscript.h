#ifndef BUILD_SCRIPT_INCLUDED
#define BUILD_SCRIPT_INCLUDED

#include "../grinliz/glstringutil.h"

struct BuildData
{
	const char*			cName;
	const char*			cStructure;
	int					techLevel;
	int					cost;
	int					size;			// 1 or 2
	grinliz::IString	name;			// "Vault"
	grinliz::IString	label;			// "Vault\n50"
	grinliz::IString	structure;		// "vault"
};


// Utility class. Cheap to create.
class BuildScript
{
public:
	BuildScript()	{}

	enum {
		NONE,
		CLEAR,
		ROTATE,
		PAVE,
		ICE,
		KIOSK_N,
		KIOSK_M,
		KIOSK_C,
		KIOSK_S,
		VAULT,
		FACTORY,
		NUM_OPTIONS,

		NUM_TECH_LEVELS = 4,

		TECH_UTILITY = 0,
		TECH0,
		TECH1,
		TECH2,
		TECH3
	};

	static bool IsBuild( int action ) { return action >= PAVE; }
	static bool IsClear( int action ) { return action == CLEAR; }

	const BuildData& GetData( int i );
	const BuildData* GetDataFromStructure( const grinliz::IString& structure );

private:
	static BuildData buildData[NUM_OPTIONS];
};

#endif // BUILD_SCRIPT_INCLUDED
