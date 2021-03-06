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

#ifndef LUMOS_BOLT_INCLUDED
#define LUMOS_BOLT_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glcolor.h"
#include "../grinliz/glcontainer.h"

#include "vertex.h"
#include "shadermanager.h"

class Engine;
struct Bolt;
struct ModelVoxel;
class GPUVertexBuffer;

class IBoltImpactHandler
{
public:
	virtual void HandleBolt( const Bolt& bolt, const ModelVoxel& mv ) = 0;
};


struct Bolt {
	Bolt() {
		head.Set( 0, 0, 0 );
		len = 0;
		dir.Set( 1, 0, 0 );
		impact = false;
		color.Set( 1, 1, 1, 1 );
		speed = 1.0f;
		particle = false;
		frameCount = 1000;

		chitID = 0;
	} 
	~Bolt() {}

	void Serialize( XStream* );

	grinliz::Vector3F	head;
	float				len;
	grinliz::Vector3F	dir;	// normal vector
	bool				impact;	// 'head' is the impact location
	grinliz::Vector4F	color;
	float				speed;
	bool				particle;
	int					frameCount;

	// Userdata follows
	int		chitID;		// who fired this bolt
	float	damage;
	int		effect;

	static void TickAll( grinliz::CDynArray<Bolt>* bolts, U32 delta, Engine* engine, IBoltImpactHandler* handler );
};


class BoltRenderer : public IDeviceLossHandler
{
public:
	BoltRenderer();
	virtual ~BoltRenderer();

	void DrawAll( const Bolt* bolts, int nBolts, Engine* engine );
	virtual void DeviceLoss();

private:
	enum { MAX_BOLTS = 400 };
	int nBolts;

	PTCVertex			vertex[MAX_BOLTS*4];
	GPUVertexBuffer*	vbo;
};

#endif // LUMOS_BOLT_INCLUDED
