#ifndef BATTERY_COMPONENT_INCLUDED
#define BATTERY_COMPONENT_INCLUDED

#include "../xegame/component.h"
#include "../xegame/cticker.h"
#include "../gamui/gamui.h"

class BatteryComponent : public Component
{
private:
	typedef Component super;

public:
	BatteryComponent() : charge(0)	{}
	virtual ~BatteryComponent()	{}

	virtual const char* Name() const { return "BatteryComponent"; }

	virtual void Serialize( XStream* xs );

	virtual void OnAdd(Chit* chit, bool initialize);
	virtual void OnRemove();

	virtual void DebugStr( grinliz::GLString* str )		{ str->AppendFormat( "[Battery] %d ", charge ); }
	virtual int DoTick( U32 delta );
	virtual void OnChitMsg( Chit* chit, const ChitMsg& msg );

	int Charge() const { return (int)charge; }
	bool UseCharge();

private:
	float charge;
	gamui::DigitalBar bar;
};


#endif // BATTERY_COMPONENT_INCLUDED