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

#ifndef EvalBuilding_SCRIPT_INCLUDED
#define EvalBuilding_SCRIPT_INCLUDED

#include "scriptcomponent.h"
#include "../xegame/cticker.h"

// FIXME: this is a strange class; it should
// probably be part of the MapSpatialComponent (which
// in turn should have the BuildingSpatialComponent
// factored out.) However, at this time, I'm concered
// that the spatial component will be going away for
// performance reasons and I'm hesitant to add code there.

class EvalBuildingScript : public IScript
{
public:
	EvalBuildingScript() : lastEval(0), eval(0), timer(2000) {}
	virtual ~EvalBuildingScript()	{}

	virtual void Init()			{}
	virtual void OnAdd();
	virtual void OnRemove()		{}
	virtual void Serialize(XStream* xs);
	virtual int DoTick(U32 delta);
	virtual const char* ScriptName()	{ return "EvalBuildingScript"; }

	virtual EvalBuildingScript* ToEvalBuildingScript() { return this; }
	double EvalBuildingScript::EvalIndustrial(bool debugLog);

private:
	CTicker timer;
	U32		lastEval;
	double	eval;
};

#endif // EvalBuilding_SCRIPT_INCLUDED